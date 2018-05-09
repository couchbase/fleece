//
//  sliceIO.hh
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

#pragma once
#include "slice.hh"
#include <string>
#include <stdio.h>

namespace fleece {

    alloc_slice readFile(const char *path);

    void writeToFile(slice s, const char *path, int mode);
    void writeToFile(slice s, const char *path);
    void appendToFile(slice s, const char *path);


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
