//
//  SupportTests.cc
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

#include "FleeceTests.hh"
#include "FleeceImpl.hh"
#include "Bitmap.hh"
#include "Function.hh"
#include "TempArray.hh"
#include "sliceIO.hh"
#include <iostream>

using namespace std;


// TESTS:

#ifndef _MSC_VER

template <class T>
static void stackEm(size_t n, bool expectedOnHeap) {
    cerr << "TempArray[" << n << "] -- " << n*sizeof(T) << " bytes, on "
         << (expectedOnHeap ? "heap" : "stack") << "\n";
    int64_t before = -1;
    TempArray(myArray, T, n);
    int64_t after = -1;

    CHECK((sizeof(myArray[0]) == sizeof(T)));
    CHECK(myArray._onHeap == expectedOnHeap);
    for (size_t i = 0; i < n; i++)
        myArray[i] = 0;
    REQUIRE(before == -1);
    REQUIRE(after == -1);
}


TEST_CASE("TempArray") {
    // Edge cases:
    stackEm<uint8_t>(0, false);
    stackEm<uint8_t>(1, false);
    stackEm<uint8_t>(1023, false);
    stackEm<uint8_t>(1024, true);

#if FL_EMBEDDED
    static constexpr size_t kBigSize = 10000;
#else
    static constexpr size_t kBigSize = 10000000;
#endif

    // A range of sizes:
    for (size_t n = 1; n < kBigSize; n *= 7)
        stackEm<uint8_t>(n, n >= 1024);
    for (size_t n = 1; n < kBigSize; n *= 7)
        stackEm<uint64_t>(n, n >= 1024/8);
}

#endif


#if FL_HAVE_FILESYSTEM
TEST_CASE("Slice I/O") {
    const char *filePath = kTempDir "slicefile";
    slice data = "This is some data to write to a file."_sl;
    writeToFile(data, filePath);
    alloc_slice readBack = readFile(filePath);
    CHECK(readBack == data);

#if FL_HAVE_MMAP
    FILE *f = fopen(filePath, "r");
    mmap_slice mappedData(f, 300);
    CHECK(slice(mappedData.buf, data.size) == data);
#endif

    appendToFile(" More data appended."_sl, filePath);
    readBack = readFile(filePath);
    CHECK(readBack == "This is some data to write to a file. More data appended."_sl);

#if FL_HAVE_MMAP
    CHECK(slice(mappedData.buf, readBack.size) == readBack);
    fclose(f);
#endif
}
#endif


TEST_CASE("Bitmap") {
    CHECK(popcount(0) == 0);
    CHECK(popcount(0l) == 0);
    CHECK(popcount(0ll) == 0);
    CHECK(popcount(-1) == sizeof(int)*8);
    CHECK(popcount(-1l) == sizeof(long)*8);
    CHECK(popcount(-1ll) == sizeof(long long)*8);

    Bitmap<uint32_t> b(0x12345678);
    CHECK(Bitmap<uint32_t>::capacity == 32);
    CHECK(!b.empty());
    CHECK(b.bitCount() == 13);
    CHECK(b.indexOfBit(8) == 4);
}


static Function<double(double)> add(double n) {
    // Return a lambda that captures a string, to make sure copying works correctly.
    // And add some doubles to capture, to increase the lambda's size.
    double a = 1, b = 2;
    return [=](double n2) {
        if (a > b) return 0.0;
        return n + n2;
    };
}

TEST_CASE("Function with primitive types") {
    Function<double(double)> fn;
    CHECK(!fn);

    fn = add(100);
    CHECK(fn);
    CHECK(fn(1) == 101);

    auto fn2(fn);
    CHECK(fn2(200) == 300);
    CHECK(fn(1000) == 1100);

    auto fn3(std::move(fn));
    CHECK(fn3(23) == 123);
    CHECK(!fn);
}


static Function<string(const string&)> append(string str) {
    // Return a lambda that captures a string, to make sure copying works correctly.
    // And add some doubles to capture, to increase the lambda's size.
    double a = 1, b = 2;
    return [=](const string &s) {
        if (a > b) return string("?");
        return str + s;
    };
}

TEST_CASE("Function with non-primitive types") {
    Function<string(const string&)> fn;
    CHECK(!fn);

    fn = append("Hello ");
    CHECK(fn);
    CHECK(fn("World") == "Hello World");

    auto fn2(fn);
    CHECK(fn2("Mom") == "Hello Mom");
    CHECK(fn("World") == "Hello World");

    auto fn3(std::move(fn));
    CHECK(fn3("Baby") == "Hello Baby");
    CHECK(!fn);
}
