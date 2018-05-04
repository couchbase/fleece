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
