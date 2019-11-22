//
// FLSlice.cc
//
// Copyright © 2019 Couchbase. All rights reserved.
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

#include "fleece/FLSlice.h"
#include <atomic>
#include <cstddef>
#include "betterassert.hh"


bool FLSlice_Equal(FLSlice a, FLSlice b) FLAPI {
    return a.size==b.size && memcmp(a.buf, b.buf, a.size) == 0;
}


int FLSlice_Compare(FLSlice a, FLSlice b) FLAPI {
    // Optimized for speed
    if (a.size == b.size)
        return memcmp(a.buf, b.buf, a.size);
    else if (a.size < b.size) {
        int result = memcmp(a.buf, b.buf, a.size);
        return result ? result : -1;
    } else {
        int result = memcmp(a.buf, b.buf, b.size);
        return result ? result : 1;
    }
}


namespace fleece {

#ifndef NDEBUG
#if FL_EMBEDDED
    static constexpr size_t kHeapAlignmentMask = 0x03;
#else
    static constexpr size_t kHeapAlignmentMask = 0x07;
#endif
    static inline bool isHeapAligned(const void *p) {
        return ((size_t)p & kHeapAlignmentMask) == 0;
    }
#endif


    // FL_DETECT_COPIES enables a check for unnecessary copying of alloc_slice memory: situations
    // where an alloc_slice gets downcast to a slice, which is then used to construct another
    // alloc_slice. If you can fix the calling code to avoid the downcast, the alloc_slice
    // constructor will just retain instead of copying.
    // This mode is incompatible with sanitizer tools like Clang's Address Sanitizer or valgrind,
    // since it peeks outside the bounds of the input slice, which is very likely to trigger
    // warnings about buffer overruns.
#ifndef FL_DETECT_COPIES
#define FL_DETECT_COPIES 0
#endif


    // The heap-allocated buffer that an alloc_slice points to.
    // It's ref-counted; every alloc_slice manages retaining/releasing its sharedBuffer.
    struct sharedBuffer {
        std::atomic<uint32_t> _refCount {1};
#if FL_DETECT_COPIES
        static constexpr uint32_t kMagic = 0xdecade55;
        uint32_t const _magic {kMagic};
#endif
        uint8_t _buf[4];

        static inline void* operator new(size_t basicSize, size_t bufferSize) {
            return malloc(basicSize - sizeof(sharedBuffer::_buf) + bufferSize);
        }

        static inline void operator delete(void *self) {
            assert(isHeapAligned(self));
            free(self);
        }

        inline void retain() noexcept {
            assert(isHeapAligned(this));
            ++_refCount;
        }

        inline void release() noexcept {
            assert(isHeapAligned(this));
            if (--_refCount == 0)
                delete this;
        }
    };

    static sharedBuffer* bufferFromBuf(const void *buf) {
        return (sharedBuffer*)((uint8_t*)buf  - offsetof(sharedBuffer, _buf));
    }

}

using namespace fleece;


FLSliceResult FLSliceResult_New(size_t size) FLAPI {
    auto sb = new (size) sharedBuffer;
    if (!sb)
        return {};
    return {&sb->_buf, size};
}


FLSliceResult FLSlice_Copy(FLSlice s) FLAPI {
    if (!s.buf)
        return {};
#if FL_DETECT_COPIES
    // Warn if s appears to be the buffer of an existing alloc_slice:
    if (s.buf && isHeapAligned(s.buf)
        && ((size_t)s.buf & 0xFFF) >= 4   // reading another VM page may crash
        && ((const uint32_t*)s.buf)[-1] == kMagic) {
        fprintf(stderr, "$$$$$ Copying existing alloc_slice at {%p, %zu}\n", s.buf, s.size);
    }
#endif
    auto sb = new (s.size) sharedBuffer;
    if (!sb)
        return {};
    memcpy(&sb->_buf, s.buf, s.size);
    return {&sb->_buf, s.size};
}


void _FLBuf_Retain(const void *buf) FLAPI {
    if (buf)
        bufferFromBuf(buf)->retain();
}


void _FLBuf_Release(const void *buf) FLAPI {
    if (buf)
        bufferFromBuf(buf)->release();
}


