//
//  mmap_slice.cc
//  Fleece
//
//  Created by Jens Alfke on 4/27/18.
//Copyright © 2018 Couchbase. All rights reserved.
//

#include "sliceIO.hh"
#include "PlatformCompat.hh"
#include <fcntl.h>
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
        assert(fd != -1);
        struct stat stat;
        fstat(fd, &stat);
        alloc_slice data(stat.st_size);
        __unused ssize_t bytesRead = ::read(fd, (void*)data.buf, data.size);
        assert(bytesRead == (ssize_t)data.size);
        ::close(fd);
        return data;
    }

    void writeToFile(slice s, const char *path, int mode) {
        int fd = ::open(path, mode | O_WRONLY | O_BINARY, 0600);
        assert(fd != -1);
        __unused ssize_t written = ::write(fd, s.buf, s.size);
        assert(written == (ssize_t)s.size);
        ::close(fd);
    }

    void writeToFile(slice s, const char *path) {
        writeToFile(s, path, O_CREAT | O_TRUNC);
    }


    void appendToFile(slice s, const char *path) {
        writeToFile(s, path, O_CREAT | O_APPEND);
    }


#ifndef _MSC_VER

    mmap_slice::mmap_slice()
    :_fd(-1)
    { }

    mmap_slice::mmap_slice(const char *path)
    :_fd(-1)
    {
        _fd = ::open(path, O_RDONLY);
        if (_fd < 0)
            return;
        struct stat stat;
        if (::fstat(_fd, &stat) < 0)
            return;
        const void* mapped = ::mmap(nullptr, stat.st_size, PROT_READ, MAP_PRIVATE, _fd, 0);
        if (mapped == MAP_FAILED)
            return;
        setBuf(mapped);
        setSize(stat.st_size);
    }

    mmap_slice::~mmap_slice() {
        reset();
    }

    mmap_slice& mmap_slice::operator= (mmap_slice&& other) {
        reset();
        _fd = other._fd;
        set(other.buf, other.size);
        other._fd = -1;
        other.set(nullptr, 0);
        return *this;
    }

    void mmap_slice::reset() {
        if (buf)
            munmap((void*)buf, size);
        if (_fd != -1) {
            close(_fd);
            _fd = -1;
        }
        set(nullptr, 0);
    }

#else

    mmap_slice::mmap_slice(const char *path)
    :_fileHandle{}
    ,_mapHandle{}
    {
        _fileHandle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        LARGE_INTEGER size;
        const BOOL gotSize = GetFileSizeEx(_fileHandle, &size);
        if (gotSize == 0) {
            return;
        }

        setSize(size.QuadPart);
        _mapHandle = CreateFileMappingA(_fileHandle, nullptr, PAGE_READONLY, size.HighPart, size.LowPart, "FileMappingObject");
        const void* mapped = MapViewOfFile(_mapHandle, FILE_MAP_READ, 0, 0, size.QuadPart);
        assert(mapped != nullptr);
        setBuf(mapped);
    }


    mmap_slice::~mmap_slice() {
        if (buf)
            UnmapViewOfFile(buf);
        if (_mapHandle)
            CloseHandle(_mapHandle);
        if (_fileHandle)
            CloseHandle(_fileHandle);
        }
    }

#endif

}
