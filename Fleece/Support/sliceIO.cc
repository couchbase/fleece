//
// sliceIO.cc
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "sliceIO.hh"

#if FL_HAVE_FILESYSTEM

#include "FleeceException.hh"
#include "fleece/PlatformCompat.hh"
#include "NumConversion.hh"
#include <fcntl.h>
#include <errno.h>

#ifndef WIN32
    #include <sys/stat.h>
    #include <unistd.h>
    #define _open open
    #define _close close
    #define _write write
    #define _read read
#else
    #include <io.h>
    #include <Windows.h>
#endif

#ifndef WIN32
#define O_BINARY 0
#endif


namespace fleece {

    alloc_slice readFile(const char *path) {
        int fd = ::_open(path, O_RDONLY | O_BINARY);
        if (fd < 0)
            FleeceException::_throwErrno("Can't open file %s", path);
        struct stat stat;
        fstat(fd, &stat);
        if (stat.st_size > SIZE_MAX)
            throw std::logic_error("File too big for address space");
        alloc_slice data(narrow_cast<size_t>(stat.st_size));
        ssize_t bytesRead = narrow_cast<ssize_t>(::_read(fd, (void*)data.buf, narrow_cast<unsigned int>(data.size)));
        if (bytesRead < narrow_cast<ssize_t>(data.size))
            FleeceException::_throwErrno("Can't read file %s", path);
        ::_close(fd);
        return data;
    }

    void writeToFile(slice s, const char *path, int mode) {
        int fd = ::_open(path, mode | O_WRONLY | O_BINARY, 0600);
        if (fd < 0)
            FleeceException::_throwErrno("Can't open file");
        ssize_t written = narrow_cast<ssize_t>(::_write(fd, s.buf, narrow_cast<unsigned int>(s.size)));
        if(written < narrow_cast<ssize_t>(s.size))
            FleeceException::_throwErrno("Can't write file");
        ::_close(fd);
    }

    void writeToFile(slice s, const char *path) {
        writeToFile(s, path, O_CREAT | O_TRUNC);
    }


    void appendToFile(slice s, const char *path) {
        writeToFile(s, path, O_CREAT | O_APPEND);
    }

}

#endif // FL_HAVE_FILESYSTEM
