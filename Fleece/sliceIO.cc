//
//  mmap_slice.cc
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

#include "sliceIO.hh"
#include "FleeceException.hh"
#include "PlatformCompat.hh"
#include <fcntl.h>
#include <errno.h>
#ifndef _MSC_VER
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <unistd.h>
    #define O_BINARY 0
#else
    #include <io.h>
    #include <windows.h>
#endif


namespace fleece {

    alloc_slice readFile(const char *path) {
        int fd = ::open(path, O_RDONLY | O_BINARY);
        if (fd < 0)
            FleeceException::_throwErrno("Can't open file");
        struct stat stat;
        fstat(fd, &stat);
        if (stat.st_size > SIZE_MAX)
            throw std::logic_error("File too big for address space");
        alloc_slice data((size_t)stat.st_size);
        __unused ssize_t bytesRead = ::read(fd, (void*)data.buf, data.size);
        if (bytesRead < (ssize_t)data.size)
            FleeceException::_throwErrno("Can't read file");
        ::close(fd);
        return data;
    }

    void writeToFile(slice s, const char *path, int mode) {
        int fd = ::open(path, mode | O_WRONLY | O_BINARY, 0600);
        if (fd < 0)
            FleeceException::_throwErrno("Can't open file");
        __unused ssize_t written = ::write(fd, s.buf, s.size);
        if(written < (ssize_t)s.size)
            FleeceException::_throwErrno("Can't write file");
        ::close(fd);
    }

    void writeToFile(slice s, const char *path) {
        writeToFile(s, path, O_CREAT | O_TRUNC);
    }


    void appendToFile(slice s, const char *path) {
        writeToFile(s, path, O_CREAT | O_APPEND);
    }


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
