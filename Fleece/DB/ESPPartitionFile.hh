//
// ESPPartitionFile.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#ifdef FL_ESP32
#include "slice.hh"
#include "PlatformCompat.hh"
#include <stdio.h>
#include "esp_partition.h"

namespace fleece { namespace ESP32 {

    /** A simple implementation of FILE that accesses an entire partition. */
    class PartitionFile {
    public:
        static FILE* open(const esp_partition_t *partition NONNULL,
                          const void *mappedMemory,
                          const char *mode,
                          size_t bufferSize =0);

    private:

        PartitionFile(const esp_partition_t *p, const void *mappedMemory);
        ~PartitionFile();

        FILE* open(const char *mode);
        void saveState() noexcept;
        static int read_callback(void *cookie, char *buf, int nbytes) noexcept;
        int read(char *buf, int nbytes) noexcept;
        static int write_callback(void *cookie, const char *buf, int nbytes) noexcept;
        int write(const char *buf, int nbytes) noexcept;
        static fpos_t lseek_callback(void *cookie, fpos_t offset, int whence) noexcept;
        fpos_t lseek(fpos_t offset, int whence) noexcept;
        static int close_callback(void *cookie) noexcept;

        struct PersistentState {
            uint32_t magic;     // Must be kMagic
            uint32_t start;     // Offset where file data starts
            uint32_t erased;    // Last erased offset
            uint32_t end;       // Offset where file data ends

            static constexpr uint32_t kMagic = 0x01ceeef1;

            void initialize()           {magic = kMagic;start = end = erased = 0;}
            bool isInitialized() const  {return magic == kMagic;}
            bool isValid() const        {return isInitialized() && start <= end && erased >= end
                                                && (erased % SPI_FLASH_SEC_SIZE) == 0;}
        };

        PersistentState         _state;
        const esp_partition_t*  _partition;
        const void*             _mappedMemory;
        uint32_t /*nvs_handle*/ _nvsHandle {0};
        uint32_t                _pos;
        bool                    _append;
        bool                    _overwrite;
    };

} }

#endif // FL_ESP32
