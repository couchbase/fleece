//
//  FleeceTests.hh
//  Fleece
//
//  Created by Jens Alfke on 11/14/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//

#pragma once

#include "Fleece.hh"
#include "Benchmark.hh"
#include "JSON5.hh"
#include <ostream>

using namespace fleece;

// Directory containing test files:
#define kTestFilesDir "Tests/"

// Less-obnoxious names for cppunit assertions:
#define Assert CPPUNIT_ASSERT
#define AssertEqual(ACTUAL, EXPECTED) CPPUNIT_ASSERT_EQUAL(EXPECTED, ACTUAL)


// Some operators to make slice work with AssertEqual:
namespace fleece {
    std::ostream& operator<< (std::ostream& o, slice s);
}

std::string sliceToHex(slice);
std::string sliceToHexDump(slice, size_t width = 16);


alloc_slice readFile(const char *path);
void writeToFile(slice s, const char *path);


struct mmap_slice : public pure_slice {
    mmap_slice(const char *path);
    ~mmap_slice();

    operator slice()    {return {buf, size};}

private:
    int _fd;
    void* _mapped;
    mmap_slice(const mmap_slice&);
};


// Converts JSON5 to JSON; helps make JSON test input more readable!
static inline std::string json5(const std::string &s)      {return fleece::ConvertJSON5(s);}



#include "catch.hpp"
