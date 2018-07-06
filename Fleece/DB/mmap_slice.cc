//
// mmap_slice.cc
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

#include "mmap_slice.hh"
#include "FleeceException.hh"
#include "PlatformCompat.hh"
#include <fcntl.h>
#include <errno.h>

#if FL_HAVE_MMAP

#ifdef FL_ESP32
#elif _MSC_VER
    #include <io.h>
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif


namespace fleece {

    mmap_slice::mmap_slice(FILE *f, size_t size) {
        const void *mapping;
#ifdef _MSC_VER
        HANDLE fileHandle = ???;
        LARGE_INTEGER sz;
        sz.QuadPart = size;
        _mapHandle = CreateFileMappingA(_fileHandle, nullptr, PAGE_READONLY,
                                        sz.HighPart, sz.LowPart, "FileMappingObject");
        mapping = MapViewOfFile(_mapHandle, FILE_MAP_READ, 0, 0, sz.QuadPart) );
        if (mapping == nullptr)
            FleeceException::_throwErrno("Can't memory-map file");
#else
        // Note: essential to use MAP_SHARED instead of MAP_PRIVATE; otherwise if the file is
        // written to thru `f`, changes in the file may not be reflected in the mapped memory!
        mapping = ::mmap(nullptr, size, PROT_READ, MAP_SHARED, fileno(f), 0);
        if (mapping == MAP_FAILED)
            FleeceException::_throwErrno("Can't memory-map file");
#endif
        set(mapping, size);
    }

    mmap_slice::~mmap_slice() {
        try {
            unmap();
        } catch (...) { }
    }

    mmap_slice& mmap_slice::operator= (mmap_slice&& other) noexcept {
        set(other.buf, other.size);
        other.set(nullptr, 0);
#ifdef _MSC_VER
        _mapHandle = other._mapHandle;
        other._mapHandle = nullptr;
#endif
        return *this;
    }

    void mmap_slice::unmap() {
#ifdef _MSC_VER
        if (buf)
            UnmapViewOfFile(buf);
        if (_mapHandle) {
            CloseHandle(_mapHandle);
            _mapHandle = nullptr;
        }
#else
        if (buf) {
            if (munmap((void*)buf, size) != 0)
                FleeceException::_throwErrno("Can't unmap memory");
        }
#endif
        set(nullptr, 0);
    }

}

#endif // FL_HAVE_MMAP
