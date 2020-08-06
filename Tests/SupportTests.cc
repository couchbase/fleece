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
#include "ConcurrentMap.hh"
#include "Bitmap.hh"
#include "TempArray.hh"
#include "sliceIO.hh"
#include <iostream>
#include <future>

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


TEST_CASE("ConcurrentMap basic", "[ConcurrentMap]") {
    ConcurrentMap map(2048);
    cout << "table size = " << map.tableSize() << ", capacity = " << map.capacity() << '\n';
    CHECK(map.count() == 0);
    CHECK(map.capacity() >= 2048);

    CHECK(map.find("apple").key == nullptr);
    auto apple = map.insert("apple", 0x4667e);
    CHECK(apple.keySlice() == "apple"_sl);
    CHECK(apple.value == 0x4667e);

    auto found = map.find("apple");
    CHECK(found.key == apple.key);
    CHECK(found.value == apple.value);

    // second insert should return the original value:
    found = map.insert("apple", 0xdeadbeef);
    CHECK(found.key == apple.key);
    CHECK(found.value == apple.value);

    for (int pass = 1; pass <= 2; ++pass) { // insert on 1st pass, read on 2nd
        for (int i = 0; i < 2046; ++i) {
            char keybuf[10];
            sprintf(keybuf, "k-%04d", i);
            auto result = (pass == 1) ? map.insert(keybuf, i) : map.find(keybuf);
            CHECK(result.keySlice() == keybuf);
            CHECK(result.value == i);
        }
    }
    
    cout << "max probes = " << map.maxProbes() << '\n';
    map.dump();
}


TEST_CASE("ConcurrentMap concurrency", "[ConcurrentMap]") {
    static constexpr size_t kSize = 1000000;
    ConcurrentMap map(kSize + 100);
    cout << "table size = " << map.tableSize() << ", capacity = " << map.capacity() << '\n';
    CHECK(map.count() == 0);
    CHECK(map.capacity() >= kSize);

    vector<string> keys;
    for (int i = 0; i < kSize; i++) {
        char keybuf[10];
        sprintf(keybuf, "k-%d", i);
        keys.push_back(keybuf);
    }

    auto reader = [&](int step) {
        size_t index = random() % kSize;
        for (int n = 0; n < kSize; n++) {
            auto e = map.find(keys[index].c_str());
            if (e.key) {
                assert(e.keySlice() == keys[index].c_str());
                assert(e.value == index);
            }
            index = (index + step) % kSize;
        }
    };

    auto writer = [&](int step) { // step must be coprime with kSize
        unsigned index = (unsigned)random() % kSize;
        for (int n = 0; n < kSize; n++) {
            auto e = map.insert(keys[index].c_str(), index);
            assert(e.keySlice() == keys[index].c_str());
            assert(e.value == index);
            index = (index + step) % kSize;
        }
    };

    auto f1 = async(reader, 7);
    auto f2 = async(reader, 53);
    auto f3 = async(writer, 23);
    auto f4 = async(writer, 91);

    f1.wait();
    f2.wait();
    f3.wait();
    f4.wait();

    CHECK(map.count() == kSize);
    cout << "max probes = " << map.maxProbes() << '\n';
}
