//
//  SupportTests.cc
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "fleece/FLBase.h"
#include "FleeceTests.hh"
#include "FleeceImpl.hh"
#include "ConcurrentMap.hh"
#include "Bitmap.hh"
#include "TempArray.hh"
#include "sliceIO.hh"
#include "Base64.hh"
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


TEST_CASE("Hash distribution") {
    static constexpr int kSize = 4096, kNKeys = 2048;
    int bucket[kSize] = {0};
    constexpr size_t bufSize = 10;
    for (int i = 0; i < kNKeys; ++i) {
        char keybuf[bufSize];
        snprintf(keybuf, bufSize, "k-%04d", i);
        int hash = slice(keybuf).hash();
        int index = hash & (kSize-1);
        ++bucket[index];
    }

    int hist[kSize] = {0};
    for (int i = 0; i < kSize; ++i) {
        ++hist[bucket[i]];
    }
    int total = 0;
    for (int i = kSize - 1; i >= 0; --i) {
        if (hist[i] > 0 || total > 0) {
            cout << hist[i] << " buckets have " << i << " keys\n";
            total += i * hist[i];
            CHECK(i <= 7);
        }
    }
    CHECK(total == kNKeys);
}


#pragma mark - CONCURRENT MAP:


TEST_CASE("ConcurrentMap basic", "[ConcurrentMap]") {
    ConcurrentMap map(2048);
    cout << "table size = " << map.tableSize() << ", capacity = " << map.capacity()
         << ", strings capacity = " << map.stringBytesCapacity() << '\n';
    CHECK(map.count() == 0);
    CHECK(map.capacity() >= 2048);
    CHECK(map.stringBytesCount() == 0);
    CHECK(map.stringBytesCapacity() >= 2048 * 16);

    CHECK(map.find("apple").key == nullptr);
    auto apple = map.insert("apple", 0x4667);
    CHECK(apple.key == "apple"_sl);
    CHECK(apple.value == 0x4667);

    auto found = map.find("apple");
    CHECK(found.key == apple.key);
    CHECK(found.value == apple.value);

    // second insert should return the original value:
    found = map.insert("apple", 0xdead);
    CHECK(found.key == apple.key);
    CHECK(found.value == apple.value);

    // nonexistent key:
    found = map.find("durian");
    CHECK(!found.key);
    CHECK(!map.remove("durian"));

    constexpr size_t bufSize = 10;
    
    for (int pass = 1; pass <= 2; ++pass) { // insert on 1st pass, read on 2nd
        for (uint16_t i = 0; i < 2046; ++i) {
            char keybuf[bufSize];
            snprintf(keybuf, bufSize, "k-%04d", i);
            auto result = (pass == 1) ? map.insert(keybuf, i) : map.find(keybuf);
            CHECK(result.key == keybuf);
            CHECK(result.value == i);
        }
    }

    // now remove a key:
    CHECK(map.remove("apple"));
    found = map.find("apple");
    CHECK(!found.key);

    cout << "Afterwards: count = " << map.count()
         << ", string bytes used = " << map.stringBytesCount() << '\n';

    CHECK(map.count() == 2046);
    CHECK(map.stringBytesCount() > 0);

    //map.dump();
}


TEST_CASE("ConcurrentMap concurrency", "[ConcurrentMap]") {
    static constexpr size_t kSize = 6000;
    ConcurrentMap map(kSize);
    cout << "table size = " << map.tableSize() << ", capacity = " << map.capacity() << '\n';
    cout << "string capacity = " << map.stringBytesCapacity()
         << ", used = " << map.stringBytesCount() << "\n";
    CHECK(map.count() == 0);
    CHECK(map.capacity() >= kSize);
    CHECK(map.stringBytesCapacity() >= 65535);

    vector<string> keys;
    constexpr size_t bufSize = 10;
    for (int i = 0; i < kSize; i++) {
        char keybuf[bufSize];
        snprintf(keybuf, bufSize, "%x", i);
        keys.push_back(keybuf);
    }

    // Note: cannot use CHECK or REQUIRE in lambdas below because the Catch test framework is not
    //       thread-safe :(  Thus `assert` is used instead.

    auto reader = [&](int step) {
        size_t index = random() % kSize;
        for (int n = 0; n < 2 * kSize; n++) {
            auto e = map.find(keys[index].c_str());
            if (e.key) {
                assert(e.key == keys[index].c_str());
                assert(e.value == index);
            }
            index = (index + step) % kSize;
        }
    };

    auto writer = [&](int step, bool deleteToo) { // step must be coprime with kSize
        unsigned const startIndex = (unsigned)random() % kSize;
        auto index = startIndex;
        for (int n = 0; n < kSize; n++) {
            auto value = uint16_t(index & 0xFFFF);
            auto e = map.insert(keys[index].c_str(), value);
            if (e.key == nullslice) {
                cerr << "CONCURRENTMAP OVERFLOW, strings used=" << map.stringBytesCount()
                     << ", keys = " << map.count() << "\n";
                throw runtime_error("ConcurrentMap overflow");
            }
            assert(e.key == keys[index].c_str());
            assert(e.value == value);
            index = (index + step) % kSize;
        }
        if (deleteToo) {
            index = startIndex;
            for (int n = 0; n < kSize; n++) {
                map.remove(keys[index].c_str());
                index = (index + step) % kSize;
            }
        }
    };

    auto f1 = async(reader, 7);
    auto f2 = async(reader, 53);
    auto f3 = async(writer, 23, true);
    auto f4 = async(writer, 91, true);

    f1.wait();
    f2.wait();
    f3.wait();
    f4.wait();

    cout << "String capacity = " << map.stringBytesCapacity() << ", used = " << map.stringBytesCount() << "\n";
    CHECK(map.count() == 0);
}


#pragma mark - SMALLVECTOR:


TEST_CASE("SmallVector, small", "[SmallVector]") {
    smallVector<alloc_slice,2> movedStrings;
    {
        smallVector<alloc_slice,2> strings;
        strings.emplace_back("string 1"_sl);
        strings.emplace_back("string 2"_sl);
        CHECK(strings.size() == 2);
        CHECK(strings[0] == "string 1"_sl);
        CHECK(strings[1] == "string 2"_sl);

        auto moveConstructedStrings(std::move(strings));
        CHECK(moveConstructedStrings.size() == 2);
        CHECK(moveConstructedStrings[0] == "string 1"_sl);
        CHECK(moveConstructedStrings[1] == "string 2"_sl);
        movedStrings = std::move(moveConstructedStrings);
    }
    CHECK(movedStrings.size() == 2);
    CHECK(movedStrings[0] == "string 1"_sl);
    CHECK(movedStrings[1] == "string 2"_sl);
}


TEST_CASE("SmallVector, big", "[SmallVector]") {
    smallVector<alloc_slice,2> movedStrings;
    {
        smallVector<alloc_slice,2> strings;
        strings.emplace_back("string 1"_sl);
        strings.emplace_back("string 2"_sl);
        strings.emplace_back("string 3"_sl);
        CHECK(strings.size() == 3);
        CHECK(strings[0] == "string 1"_sl);
        CHECK(strings[1] == "string 2"_sl);
        CHECK(strings[2] == "string 3"_sl);
        auto moveConstructedStrings(std::move(strings));
        CHECK(moveConstructedStrings.size() == 3);
        CHECK(moveConstructedStrings[0] == "string 1"_sl);
        CHECK(moveConstructedStrings[1] == "string 2"_sl);
        CHECK(moveConstructedStrings[2] == "string 3"_sl);
        movedStrings = std::move(moveConstructedStrings);
    }
    CHECK(movedStrings.size() == 3);
    CHECK(movedStrings[0] == "string 1"_sl);
    CHECK(movedStrings[1] == "string 2"_sl);
    CHECK(movedStrings[2] == "string 3"_sl);
}


TEST_CASE("Base64 encode and decode", "[Base64]") {
    vector<string> inputs = {"a", "ab", "abc", "abcd", "abcde"};
    vector<string> encodingResults = {"YQ==", "YWI=", "YWJj", "YWJjZA==", "YWJjZGU="};
    int i = 0;
    for (auto in : inputs) {
        auto encoded = base64::encode(slice(in));
        CHECK(encoded == encodingResults[i++]);
        auto decoded = base64::decode(slice(encoded));
        CHECK(decoded == in);
    }
}


TEST_CASE("Timestamp Conversions", "[Timestamps]") {
    FLTimestamp ts = FLTimestamp_Now();
    bool asUTC = true;
    SECTION("UTC") {
        asUTC = true;
    }
    SECTION("Not UTC") {
        asUTC = false;
    }
    FLStringResult str = FLTimestamp_ToString(ts, asUTC);
    FLTimestamp ts2 = FLTimestamp_FromString((FLString)str);
    CHECK(ts == ts2);
}
