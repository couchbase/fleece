//
// mmap_slice.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "sliceIO.hh"


#ifndef FL_HAVE_MMAP
#define FL_HAVE_MMAP FL_HAVE_FILESYSTEM && !FL_EMBEDDED
#endif

#if FL_HAVE_MMAP

namespace fleece {

    /** Memory-maps an open file and exposes its contents as a slice.
        The address space will be as large as the size given, even if that's larger than the file;
        this allows new parts of the file to be exposed in the mapping as data is written to it. */
    struct mmap_slice : public pure_slice {
        mmap_slice() { }
        mmap_slice(FILE* NONNULL, size_t size);
        ~mmap_slice();

        mmap_slice& operator= (mmap_slice&&) noexcept;

        operator slice() const      {return {buf, size};}

        void unmap();

    private:
        mmap_slice(const mmap_slice&) =delete;
        mmap_slice& operator= (const mmap_slice&) =delete;

#ifdef _MSC_VER
        HANDLE _mapHandle {INVALID_HANDLE_VALUE};
#endif
    };

}

#endif // FL_HAVE_MMAP
