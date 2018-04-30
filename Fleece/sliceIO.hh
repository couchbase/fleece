//
//  sliceIO.hh
//  Fleece
//
//  Created by Jens Alfke on 4/27/18.
//Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "slice.hh"

namespace fleece {

    alloc_slice readFile(const char *path);

    void writeToFile(slice s, const char *path, int mode);
    void writeToFile(slice s, const char *path);
    void appendToFile(slice s, const char *path);


    /** Memory-maps a file and exposes its contents as a slice */
    struct mmap_slice : public pure_slice {
        mmap_slice();
        explicit mmap_slice(const char *path);
        ~mmap_slice();

        mmap_slice& operator= (mmap_slice&&);

        operator slice()    {return {buf, size};}

        void reset();

    private:
        mmap_slice(const mmap_slice&) =delete;

#ifdef _MSC_VER
        HANDLE _fileHandle{INVALID_HANDLE_VALUE};
        HANDLE _mapHandle{INVALID_HANDLE_VALUE};
#else
        int _fd;
#endif
    };

}
