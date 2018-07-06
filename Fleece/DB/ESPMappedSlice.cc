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
#include "ESPPartitionFile.hh"
#include "FleeceException.hh"
#include <errno.h>

#ifdef FL_ESP32

// http://esp-idf.readthedocs.io/en/latest/api-reference/storage/spi_flash.html


namespace fleece {

    esp_mapped_slice::esp_mapped_slice(const esp_partition_t *partition NONNULL)
    :_partition(partition)
    {
        const void *mapping;
        auto err = esp_partition_mmap(partition, 0, partition->size, SPI_FLASH_MMAP_DATA,
                                      &mapping, &_mapHandle);
        if (err)
            FleeceException::_throw(InternalError, "Couldn't memory-map partition: ESP err %d", err);
        set(mapping, partition->size);
    }

    static const esp_partition_t* partitionNamed(const char *name NONNULL) {
        auto i = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, name);
        if (!i)
            FleeceException::_throw(InternalError, "esp_mapped_slice: no such partition '%s'",
                                    name);
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
        _partition = other._partition;
        _mapHandle = other._mapHandle;
        other.set(nullptr, 0);
        other._partition = nullptr;
        other._mapHandle = 0;
        return *this;
    }

    void esp_mapped_slice::unmap() {
        if (_mapHandle) {
            spi_flash_munmap(_mapHandle);
            _mapHandle = 0;
        }
        set(nullptr, 0);
    }

    FILE* esp_mapped_slice::open(const char *mode NONNULL, size_t bufferSize) {
        return ESP32::PartitionFile::open(_partition, buf, mode, bufferSize);
    }

}

#endif
