//
// ESPPartitionFile.cc
//
// Copyright © 2018 Couchbase. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "ESPPartitionFile.hh"
#include "FleeceException.hh"
#include <algorithm>
#include <errno.h>

#ifdef FL_ESP32

#include <esp_log.h>
#include <nvs.h>
#include <rom/cache.h>

#define VERBOSE 0
#define VERIFY_MAPPED_WRITES 1

#define TAG "partitionfile"
#if VERBOSE
    #define LOG(FMT, ...)    ESP_LOGI(TAG, FMT, ##__VA_ARGS__)
#else
    #define LOG(FMT, ...)    ESP_LOGD(TAG, FMT, ##__VA_ARGS__)
#endif
#define WARN(FMT, ...)   ESP_LOGW(TAG, FMT, ##__VA_ARGS__)
#define ERROR(FMT, ...)  ESP_LOGE(TAG, FMT, ##__VA_ARGS__)


// http://esp-idf.readthedocs.io/en/latest/api-reference/storage/spi_flash.html

namespace fleece { namespace ESP32 {

    FILE* PartitionFile::open(const esp_partition_t *partition NONNULL,
                              const void *mappedMemory,
                              const char *mode,
                              size_t bufferSize) {
        assert(partition != nullptr);
        LOG("open(0x%p, \"%s\")", partition, mode);
        auto pf = new PartitionFile(partition, mappedMemory);
        FILE *f = pf->open(mode);
        if (!f)
            delete pf;
        if (bufferSize > 0)
            setvbuf(f, nullptr, _IOFBF, bufferSize);
        return f;
    }


    PartitionFile::PartitionFile(const esp_partition_t *p, const void *mappedMemory)
    :_partition(p)
    ,_mappedMemory(mappedMemory)
    { }


    PartitionFile::~PartitionFile() {
        if (_nvsHandle)
            nvs_close(_nvsHandle);
    }


    FILE* PartitionFile::open(const char *mode) {
        auto err = nvs_open("PartitionFile", NVS_READWRITE, &_nvsHandle);
        if (err) {
            ERROR("can't open NVS store: %d", err);
            FleeceException::_throw(InternalError, "Couldn't open ESP NVS: err %d", err);
        }

        if (mode[0] == 'w') {
            // "w" mode resets the file to empty. Don't load any state, just init to empty;
            // new state will be saved the first time the file is written.
            _state.initialize();
            saveState();
        } else {
            size_t size = sizeof(_state);
            auto err = nvs_get_blob(_nvsHandle, _partition->label, &_state, &size);
            if (!err && size != sizeof(_state))
                err = ESP_ERR_NVS_INVALID_LENGTH;
            if (err == ESP_ERR_NVS_NOT_FOUND) {
                // Partition doesn't have a file in it
                if (mode[0] == 'r' && mode[1] != 'w') {
                    errno = ENOENT;
                    return nullptr;
                } else {
                    _state.initialize();
                    saveState();
                }
            } else if (err) {
                ERROR("nvs_get_blob failed with ESP err %d", err);
                errno = EIO;
                return nullptr;
            } else if (!_state.isValid() || _state.end > _partition->size) {
                // Hm, metadata is corrupt
                ERROR("file metadata is corrupt");
                errno = EIO;
                return nullptr;
            } else {
                LOG("    (read state: start=%x, erased=%x, end=%x)",
                         _state.start, _state.erased, _state.end);
            }
        }
        _overwrite = (strchr(mode, '*') != nullptr);
        _append = (mode[0] == 'a');
        _pos = _append ? _state.end : _state.start;
        bool writeable = (mode[0] != 'r' || mode[1] == 'w' || mode[1] == '+');
        return funopen(this,
                       &read_callback,
                       (writeable ? &write_callback : nullptr),
                       &lseek_callback,
                       &close_callback);
    }


    void PartitionFile::saveState() noexcept {
        LOG("    (saveState: start=%x, erased=%x, end=%x)",
                 _state.start, _state.erased, _state.end);
        auto err = nvs_set_blob(_nvsHandle, _partition->label, &_state, sizeof(_state));
        if (!err)
            err = nvs_commit(_nvsHandle);
        if (err) {
            ERROR("can't save state: ESP err %d", err);
            FleeceException::_throw(InternalError, "Couldn't save file metadata: ESP err %d", err);
        }
    }


    int PartitionFile::read_callback(void *cookie, char *buf, int nbytes) noexcept {
        LOG("read(%d)", nbytes);
        return ((PartitionFile*)cookie)->read(buf, nbytes);
    }


    int PartitionFile::read(char *buf, int nbytes) noexcept {
        if (_pos >= _state.end || nbytes == 0)
            return 0;
        nbytes = std::min(nbytes, (int)_state.end - (int)_pos);
        auto err = esp_partition_read(_partition, _pos, buf, nbytes);
        if (err != ESP_OK) {
            errno = EIO;
            return -1;
        }
        _pos += nbytes;
        return nbytes;
    }


    int PartitionFile::write_callback(void *cookie, const char *buf, int nbytes) noexcept {
        return ((PartitionFile*)cookie)->write(buf, nbytes);
    }


    int PartitionFile::write(const char *buf, int nbytes) noexcept {
        if (_mappedMemory)
            LOG("write(%d) at %d [%p]", nbytes, _pos, (char*)_mappedMemory + _pos);
        else
            LOG("write(%d) at %d", nbytes, _pos);

        if (_append) {
            _pos = _state.end;
        } else if (_pos < _state.end && !_overwrite) {
            // I don't support overwriting data
            ERROR("Can't overwrite data (pos=%d, EOF=%d)", _pos, _state.end);
            errno = ENXIO;
            return -1;
        }

        auto endPos = _pos + nbytes;
        if (endPos > _partition->size || endPos < _pos) {
            errno = ENOSPC;
            return -1;
        }

        bool stateChanged = false;
        if (endPos > _state.erased) {
            auto count = endPos - _state.erased;
            if (count % SPI_FLASH_SEC_SIZE > 0)
                count += SPI_FLASH_SEC_SIZE - (count % SPI_FLASH_SEC_SIZE);
            LOG("    erase [%d ... %d]", _state.erased, (_state.erased + count - 1));
            auto err = esp_partition_erase_range(_partition, _state.erased, count);
            if (err != ESP_OK) {
                errno = EIO;
                return -1;
            }
            _state.erased += count;
            stateChanged = true;
        }

        auto err = esp_partition_write(_partition, _pos, buf, nbytes);
        if (err != ESP_OK) {
            errno = EIO;
            return -1;
        }

        if (_mappedMemory) {
            // It appears to be necessary to flush CPU caches after a write,
            // to prevent reading stale bytes from the mapped memory.
            Cache_Flush(0);
            Cache_Flush(1);

#if VERIFY_MAPPED_WRITES
            assert(memcmp((char*)_mappedMemory + _pos, buf, nbytes) == 0);
#endif
        }

        _pos += nbytes;
        if (_pos > _state.end) {
            _state.end = _pos;
            stateChanged = true;
        }

        if (stateChanged)
            saveState();

        return nbytes;
    }


    fpos_t PartitionFile::lseek_callback(void *cookie, fpos_t offset, int whence) noexcept {
        LOG("seek(%ld, %d)", offset, whence);
        return ((PartitionFile*)cookie)->lseek(offset, whence);
    }


    fpos_t PartitionFile::lseek(fpos_t offset, int whence) noexcept {
        switch (whence) {
            case SEEK_SET:  offset += _state.start; break;
            case SEEK_CUR:  offset += _pos; break;
            case SEEK_END:  offset += _state.end; break;
            default:        errno = EINVAL; return -1;
        }
        if (offset < _state.start || offset > UINT32_MAX) {
            errno = EINVAL;
            return -1;
        }
        _pos = (uint32_t)offset;
        return offset - _state.start;
    }

    int PartitionFile::close_callback(void *cookie) noexcept {
        LOG("close()");
        delete (PartitionFile*)cookie;
        return 0;
    }

} }

#endif // FL_ESP32
