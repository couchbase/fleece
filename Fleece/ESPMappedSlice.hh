//
// ESPMappedSlice.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#ifdef FL_ESP32
#include "slice.hh"
#include "PlatformCompat.hh"
#include <stdio.h>
#include "esp_partition.h"

namespace fleece {

    /** Memory-maps an ESP32 Flash partition and exposes it as a slice.
        The address space will be as large as the size given, even if that's larger than the file;
        this allows new parts of the file to be exposed in the mapping as data is written to it. */
    struct esp_mapped_slice : public pure_slice {
        esp_mapped_slice() { }
        esp_mapped_slice(const esp_partition_t* NONNULL);
        esp_mapped_slice(const char* partitionName NONNULL);
        ~esp_mapped_slice();

        esp_mapped_slice& operator= (esp_mapped_slice&&) noexcept;

        operator slice() const      {return {buf, size};}

        void unmap();

        FILE* open(const char *mode NONNULL,
                   size_t bufferSize =0);

    private:
        esp_mapped_slice(const esp_mapped_slice&) =delete;
        esp_mapped_slice& operator= (const esp_mapped_slice&) =delete;

        const esp_partition_t*  _partition {nullptr};
        spi_flash_mmap_handle_t _mapHandle {0};
    };

}

#endif // FL_ESP32
