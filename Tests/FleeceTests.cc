//
//  FleeceTests.cc
//  Fleece
//
//  Created by Jens Alfke on 11/14/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//

#define CATCH_CONFIG_CONSOLE_WIDTH 120
#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file

#include "CaseListReporter.hh"

#include "FleeceTests.hh"
#include "slice.hh"
#include <assert.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace fleece;


namespace fleece {
    std::ostream& operator<< (std::ostream& o, slice s) {
        o << "slice[";
        if (s.buf == nullptr)
            return o << "null]";
        auto buf = (const uint8_t*)s.buf;
        for (size_t i = 0; i < s.size; i++) {
            if (buf[i] < 32 || buf[i] > 126)
                return o << sliceToHex(s) << "]";
        }
        return o << "\"" << std::string((char*)s.buf, s.size) << "\"]";
    }
}


std::string sliceToHex(slice result) {
    std::string hex;
    for (size_t i = 0; i < result.size; i++) {
        char str[4];
        sprintf(str, "%02X", result[i]);
        hex.append(str);
        if (i % 2 && i != result.size-1)
            hex.append(" ");
    }
    return hex;
}


std::string sliceToHexDump(slice result, size_t width) {
    std::string hex;
    for (size_t row = 0; row < result.size; row += width) {
        size_t end = std::min(row + width, result.size);
        for (size_t i = row; i < end; ++i) {
            char str[4];
            sprintf(str, "%02X", result[i]);
            hex.append(str);
            if (i % 2 && i != result.size-1)
                hex.append(" ");
        }
        hex.append("    ");
        for (size_t i = row; i < end; ++i) {
            char str[2] = {(char)result[i], 0};
            if (result[i] < 32 || result[i] >= 127)
                str[0] = '.';
            hex.append(str);
        }
        hex.append("\n");
    }
    return hex;
}


mmap_slice::mmap_slice(const char *path)
:_fd(-1),
_mapped(MAP_FAILED)
{
    _fd = ::open(path, O_RDONLY);
    assert(_fd != -1);
    struct stat stat;
    ::fstat(_fd, &stat);
    size = stat.st_size;
    _mapped = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, _fd, 0);
    assert(_mapped != MAP_FAILED);
    buf = _mapped;
}

mmap_slice::~mmap_slice() {
    if (_mapped != MAP_FAILED)
        munmap((void*)buf, size);
    if (_fd != -1)
        close(_fd);
}


alloc_slice readFile(const char *path) {
    int fd = ::open(path, O_RDONLY);
    assert(fd != -1);
    struct stat stat;
    fstat(fd, &stat);
    alloc_slice data(stat.st_size);
    ssize_t bytesRead = ::read(fd, (void*)data.buf, data.size);
    REQUIRE(bytesRead == (ssize_t)data.size);
    ::close(fd);
    return data;
}

void writeToFile(slice s, const char *path) {
    int fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    assert(fd != -1);
    ssize_t written = ::write(fd, s.buf, s.size);
    REQUIRE(written == (ssize_t)s.size);
    ::close(fd);
}
