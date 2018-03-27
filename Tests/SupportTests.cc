//
//  SupportTests.cc
//  Fleece
//
//  Created by Jens Alfke on 1/23/18.
//  Copyright © 2018 Couchbase. All rights reserved.
//

#ifndef _MSC_VER

#include "FleeceTests.hh"
#include "Fleece.hh"
#include "TempArray.hh"
#include <iostream>

using namespace std;


// TESTS:

template <class T>
static void stackEm(size_t n, bool expectedOnHeap) {
    cerr << "TempArray[" << n << "] -- " << n*sizeof(T) << " bytes, on "
         << (expectedOnHeap ? "heap" : "stack") << "\n";
    int64_t before = -1;
    TempArray(myArray, T, n);
    int64_t after = -1;

    CHECK(sizeof(myArray[0]) == sizeof(T));
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

    // A range of sizes:
    for (size_t n = 1; n < 10000000; n *= 7)
        stackEm<uint8_t>(n, n >= 1024);
    for (size_t n = 1; n < 10000000; n *= 7)
        stackEm<uint64_t>(n, n >= 1024/8);
}

#endif

