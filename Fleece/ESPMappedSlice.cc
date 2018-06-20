//
// ESPMappedSlice.cc
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

#include "ESPMappedSlice.hh"
#include "FleeceException.hh"
#include <algorithm>
#include <errno.h>
#include <iostream> //TEMP

#ifdef FL_ESP32


namespace fleece {

    namespace internal {
        // A simple implementation of FILE that accesses an entire partition:

        class PartitionFile {
        public:
            static FILE* open(const esp_partition_t *partition, const char *mode) {
                std::cerr << "open('" << mode << "')\n";//TEMP
                auto pf = new PartitionFile(partition);
                FILE *f = pf->open(mode);
                if (!f)
                    delete pf;
                return f;
            }

        private:
            struct PersistentState {
                uint32_t magic;     // Must be kMagic
                uint32_t start;     // Offset where file data starts
                uint32_t erased;    // Last erased offset
                uint32_t end;       // Offset where file data ends

                static constexpr uint32_t kMagic = 0xffceeef1;

                void initialize() {
                    magic = kMagic;
                    start = end = 0;
                    erased = 0;
                }

                bool isInitialized() const {
                    return magic == kMagic;
                }

                bool isValid() const {
                    return isInitialized() && start <= end && erased >= end
                        && (erased % SPI_FLASH_SEC_SIZE) == 0;
                }
            };

            const esp_partition_t*  _partition;
            uint32_t                _pos;
            PersistentState         _state;
            bool                    _append;

            PartitionFile(const esp_partition_t *p)
            :_partition(p)
            { }

            inline uint32_t stateOffset() const {
                return _partition->size - SPI_FLASH_SEC_SIZE;
            }

            FILE* open(const char *mode) {
                if (mode[0] == 'w') {
                    // "w" mode resets the file to empty.
                    _state.initialize();
                    saveState();
                } else {
                    auto err = esp_partition_read(_partition, stateOffset(),
                                                  &_state, sizeof(_state));
                    if (err) {
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
                        errno = EIO;
                        return nullptr;
                    }
                }
                _append = (mode[0] == 'a');
                _pos = _append ? _state.end : _state.start;
                bool writeable = (mode[0] != 'r' || mode[1] == 'w' || mode[1] == '+');
                return funopen(this,
                               &part_read,
                               (writeable ? &part_write : nullptr),
                               &part_lseek,
                               &part_close);
            }

            void saveState() noexcept {
                auto err = esp_partition_erase_range(_partition, stateOffset(), 4096);
                if (!err)
                    esp_partition_write(_partition, stateOffset(), &_state, sizeof(_state));
                if (err)
                    FleeceException::_throw(InternalError, "Couldn't save file metadata");
            }

            static int part_read(void *cookie, char *buf, int nbytes) noexcept {
                return ((PartitionFile*)cookie)->read(buf, nbytes);
            }

            int read(char *buf, int nbytes) noexcept {
                std::cerr << "read(" << nbytes << ")\n";//TEMP
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

            static int part_write(void *cookie, const char *buf, int nbytes) noexcept {
                return ((PartitionFile*)cookie)->write(buf, nbytes);
            }

            int write(const char *buf, int nbytes) noexcept {
                std::cerr << "write(" << nbytes << ")\n";//TEMP
                if (_append) {
                    _pos = _state.end;
                } else if (_pos < _state.end) {
                    // I don't support overwriting data
                    errno = ENXIO;
                    return -1;
                } else if (_pos > _state.end) {
                    // TODO: Zero-fill intervening bytes
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

            static fpos_t part_lseek(void *cookie, fpos_t offset, int whence) noexcept {
                return ((PartitionFile*)cookie)->lseek(offset, whence);
            }

            fpos_t lseek(fpos_t offset, int whence) noexcept {
                std::cerr << "seek(" << offset << ',' << whence << ")\n";//TEMP
                switch (whence) {
                    case SEEK_SET:  offset += _state.start; break;
                    case SEEK_CUR:  offset += _pos; break;
                    case SEEK_END:  offset = _state.end - offset; break;
                    default:        errno = EINVAL; return -1;
                }
                if (offset < _state.start || offset > UINT32_MAX) {
                    errno = EINVAL;
                    return -1;
                }
                _pos = (uint32_t)offset;
                return offset - _state.start;
            }

            static int part_close(void *cookie) noexcept {
                std::cerr << "close()\n";//TEMP
                delete (PartitionFile*)cookie;
                return 0;
            }
        };

    }

    
    esp_mapped_slice::esp_mapped_slice(const esp_partition_t *partition NONNULL)
    :_partition(partition)
    {
        const void *mapping;
        auto err = esp_partition_mmap(partition, 0, partition->size, SPI_FLASH_MMAP_DATA,
                                      &mapping, &_mapHandle);
        if (err)
            FleeceException::_throw(InternalError, "Couldn't memory-map partition");
        set(mapping, size);
    }

    static const esp_partition_t* partitionNamed(const char *name NONNULL) {
        auto i = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, name);
        if (!i)
            FleeceException::_throw(InternalError, "esp_mapped_slice: no such partition");
        return esp_partition_get(i);
    }

    esp_mapped_slice::esp_mapped_slice(const char *name NONNULL)
    :esp_mapped_slice(partitionNamed(name))
    { }

    esp_mapped_slice::~esp_mapped_slice() {
        unmap();
    }

    esp_mapped_slice& esp_mapped_slice::operator= (esp_mapped_slice&& other) noexcept {
        set(other.buf, other.size);
        other.set(nullptr, 0);
        return *this;
    }

    void esp_mapped_slice::unmap() {
        if (_mapHandle) {
            spi_flash_munmap(_mapHandle);
            _mapHandle = 0;
        }
        set(nullptr, 0);
    }

    FILE* esp_mapped_slice::open(const char *mode NONNULL) {
        return internal::PartitionFile::open(_partition, mode);
    }

}

#endif
