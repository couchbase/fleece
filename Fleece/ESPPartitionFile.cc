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
#include <iostream>

#ifdef FL_ESP32

#define VERBOSE 1


// http://esp-idf.readthedocs.io/en/latest/api-reference/storage/spi_flash.html

namespace fleece { namespace ESP32 {

    FILE* PartitionFile::open(const esp_partition_t *partition NONNULL,
                              const char *mode,
                              size_t bufferSize) {
        assert(partition != nullptr);
        if (VERBOSE) std::cerr << "open(0x" << (void*)partition << ", '" << mode << "')\n";
        auto pf = new PartitionFile(partition);
        FILE *f = pf->open(mode);
        if (!f)
            delete pf;
        if (bufferSize > 0)
            setvbuf(f, nullptr, _IOFBF, bufferSize);
        return f;
    }


    PartitionFile::PartitionFile(const esp_partition_t *p)
    :_partition(p)
    { }


    FILE* PartitionFile::open(const char *mode) {
        if (mode[0] == 'w') {
            // "w" mode resets the file to empty.
            _state.initialize();
            saveState();
        } else {
            auto err = esp_partition_read(_partition, stateOffset(),
                                          &_state, sizeof(_state));
            if (err) {
                std::cerr << "Warning: PartitionFile: esp_partition_read failed with err " << err << "\n";
                errno = EIO;
                return nullptr;
            } else if (!_state.isInitialized()) {
                // Partition doesn't have a file in it
                if (mode[0] == 'r' && mode[1] != 'w') {
                    errno = ENOENT;
                    return nullptr;
                } else {
                    _state.initialize();
                    saveState();
                }
            } else if (!_state.isValid() || _state.end > stateOffset()) {
                // Hm, metadata is corrupt
                std::cerr << "Warning: PartitionFile: file metadata is corrupt\n";
                errno = EIO;
                return nullptr;
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
        if (VERBOSE) std::cerr << "  (saveState: start=" << std::hex << _state.start
            << ", erased=" << _state.erased << ", end=" << _state.end << ")\n" << std::dec;
        auto err = esp_partition_erase_range(_partition, stateOffset(), 4096);
        if (!err)
        esp_partition_write(_partition, stateOffset(), &_state, sizeof(_state));
        if (err) {
            std::cerr << "Warning: PartitionFile: can't save state: ESP err " << err << "\n";
            FleeceException::_throw(InternalError, "Couldn't save file metadata");
        }
    }


    int PartitionFile::read_callback(void *cookie, char *buf, int nbytes) noexcept {
        if (VERBOSE) std::cerr << "read(" << nbytes << ")\n";
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
        if (VERBOSE) std::cerr << "write(" << nbytes << ")\n";
        return ((PartitionFile*)cookie)->write(buf, nbytes);
    }


    int PartitionFile::write(const char *buf, int nbytes) noexcept {
        if (_append) {
            _pos = _state.end;
        } else if (_pos < _state.end && !_overwrite) {
            // I don't support overwriting data
            std::cerr << "Warning: PartitionFile: Can't overwrite data (pos="
                      << _pos << ", EOF=" << _state.end << ")\n";
            errno = ENXIO;
            return -1;
        }

        auto endPos = _pos + nbytes;
        if (endPos > stateOffset() || endPos < _pos) {
            errno = ENOSPC;
            return -1;
        }

        bool stateChanged = false;
        if (endPos > _state.erased) {
            auto count = endPos - _state.erased;
            if (count % SPI_FLASH_SEC_SIZE > 0)
                count += SPI_FLASH_SEC_SIZE - (count % SPI_FLASH_SEC_SIZE);
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
        if (VERBOSE) std::cerr << "seek(" << offset << ',' << whence << ")\n";
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
        if (VERBOSE) std::cerr << "close()\n";
        delete (PartitionFile*)cookie;
        return 0;
    }

} }

#endif // FL_ESP32
