//
//  FleeceTests.hh
//  Fleece
//
//  Created by Jens Alfke on 11/14/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//

#pragma once

#include "slice.hh"
#include "JSON5.hh"
#include "Benchmark.hh"
#include <ostream>
#ifdef _MSC_VER
#include <windows.h>
#endif

using namespace fleece;

// Directory containing test files:
#ifdef _MSC_VER
#define kTestFilesDir "..\\Tests\\"
#else
#define kTestFilesDir "Tests/"
#endif

namespace fleece_test {
    std::string sliceToHex(slice);
    std::string sliceToHexDump(slice, size_t width = 16);
    std::ostream& dumpSlice(std::ostream&, slice);

    alloc_slice readFile(const char *path);
    void writeToFile(slice s, const char *path);


    struct mmap_slice : public pure_slice {
        mmap_slice(const char *path);
        ~mmap_slice();

        operator slice()    {return {buf, size};}

    private:
        void* _mapped;
    #ifdef _MSC_VER
        HANDLE _fileHandle{INVALID_HANDLE_VALUE};
        HANDLE _mapHandle{INVALID_HANDLE_VALUE};
    #else
        int _fd;
    #endif
        mmap_slice(const mmap_slice&);
    };

    // Converts JSON5 to JSON; helps make JSON test input more readable!
    static inline std::string json5(const std::string &s)      {return fleece::ConvertJSON5(s);}
}

using namespace fleece_test;

namespace fleece {
    // to make slice work with Catch's logging. This has to be in the 'fleece' namespace.
    static inline std::ostream& operator<< (std::ostream& o, slice s) {
        return dumpSlice(o, s);
    }
}

// This has to come last so that '<<' overrides can be used by Catch.
#include "catch.hpp"
