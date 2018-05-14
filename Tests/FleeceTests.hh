//
// FleeceTests.hh
//
// Copyright (c) 2015 Couchbase, Inc All rights reserved.
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
#include "JSON5.hh"
#include "sliceIO.hh"
#include "Benchmark.hh"
#include <ostream>
#ifdef _MSC_VER
#include <windows.h>
#endif

using namespace fleece;

// True if we have access to test files like "1000people.json". Predefine as 0 for embedded OS.
#ifndef FL_HAVE_TEST_FILES
#define FL_HAVE_TEST_FILES 1
#endif

// Directory containing test files:
#if FL_HAVE_TEST_FILES
    #ifdef _MSC_VER
        #define kTestFilesDir "..\\Tests\\"
        #define kTempDir "C:\\tmp"
    #else
        #define kTestFilesDir "Tests/"
        #define kTempDir "/tmp/"
    #endif
    static const char* kBigJSONTestFileName = "1000people.json";
    static const size_t kBigJSONTestCount = 1000;
#else
    #define kTestFilesDir ""
    #define kTempDir ""
    static const char* kBigJSONTestFileName = "50people.json";
    static const size_t kBigJSONTestCount = 50;
#endif

namespace fleece_test {
    std::string sliceToHex(slice);
    std::string sliceToHexDump(slice, size_t width = 16);
    std::ostream& dumpSlice(std::ostream&, slice);

    alloc_slice readTestFile(const char *path);

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
