//
// FleeceTests.hh
//
// Copyright 2015-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once

#include "fleece/slice.hh"
#include "JSON5.hh"
#include "sliceIO.hh"
#include "Benchmark.hh"
#include <ostream>
#include <cfloat>
#include <cmath>
#ifdef _MSC_VER
#include <windows.h>
#endif

#include <stdlib.h>
#if defined(_MSC_VER) || defined(ESP_PLATFORM)
    #define random rand
    #define srandom srand
#endif

using slice = fleece::slice;
using alloc_slice = fleece::alloc_slice;

#if FL_HAVE_FILESYSTEM
    #ifdef _MSC_VER
        #define kTempDir "C:\\tmp\\"
    #else
        #define kTempDir "/tmp/"
    #endif
#endif


// True if we have access to test files like "1000people.json". Predefine as 0 for embedded OS.
#ifndef FL_HAVE_TEST_FILES
#define FL_HAVE_TEST_FILES FL_HAVE_FILESYSTEM
#endif

// Directory containing test files:
#if FL_HAVE_TEST_FILES
    #if defined(LITECORE_CPP_TESTS)
        #ifdef _MSC_VER
            #ifdef __clang__
                #define kTestFilesDir "vendor\\fleece\\Tests\\"
            #else
                #define kTestFilesDir "..\\vendor\\fleece\\Tests\\"
            #endif
        #else
            #define kTestFilesDir "vendor/fleece/Tests/"
        #endif
    #else
        #ifdef _MSC_VER
            #ifdef __clang__
                #define kTestFilesDir "Tests\\"
            #else
                #define kTestFilesDir "..\\Tests\\"
            #endif
        #else
            #define kTestFilesDir "Tests/"
        #endif
    #endif
    static const char* kBigJSONTestFileName = "1000people.json";
    static const size_t kBigJSONTestCount = 1000;
#else
    #define kTestFilesDir ""
    static const char* kBigJSONTestFileName = "50people.json";
    static const size_t kBigJSONTestCount = 50;
#endif


namespace fleece_test {
    std::string sliceToHex(slice);
    std::string sliceToHexDump(slice, size_t width = 16);
    std::ostream& dumpSlice(std::ostream&, slice);

#if FL_HAVE_TEST_FILES
    alloc_slice readTestFile(const char *path);
#else
    slice readTestFile(const char *path);
#endif

    // Converts JSON5 to JSON; helps make JSON test input more readable!
    static inline std::string json5(const std::string &s)      {return fleece::ConvertJSON5(s);}
}


namespace fleece {
    // to make slice work with Catch's logging. This has to be in the 'fleece' namespace.
    static inline std::ostream& operator<< (std::ostream& o, slice s) {
        return fleece_test::dumpSlice(o, s);
    }

    static inline bool DoubleEquals(double left, double right) {
        return std::abs(left - right) <= DBL_EPSILON;
    }

    static inline bool FloatEquals(float left, float right) {
        return std::abs(left - right) <= FLT_EPSILON;
    }
}

// This has to come last so that '<<' overrides can be used by Catch.
#include "catch.hpp"
