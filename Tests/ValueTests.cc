//
//  ValueTests.cpp
//  Fleece
//
//  Created by Jens Alfke on 11/26/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//

#include "FleeceTests.hh"
#include "Value.hh"

namespace fleece {
    using namespace internal;

    class ValueTests {  // Value declares this as a friend so it can call private API
    public:

        static void testPointers() {
            Value v(4, kNarrow);
            REQUIRE(v.pointerValue<false>() == 4u);
            Value w(4, kWide);
            REQUIRE(w.pointerValue<true>() == 4u);
        }

        static void testDeref() {
            uint8_t data[6] = {0x01, 0x02, 0x03, 0x04, 0x80, 0x02};
            auto start = (const Value*)&data[4];
            REQUIRE(start->pointerValue<false>() == 4u);
            auto dst = Value::derefPointer<false>(start);
            REQUIRE((ptrdiff_t)dst - (ptrdiff_t)&data[0] == 0L);
        }

    };

    TEST_CASE("VarInt read") {
        uint8_t buf[100];
        uint64_t result;
        for (double d = 0.0; d <= UINT64_MAX; d = std::max(d, 1.0) * 1.5) {
            auto n = (uint64_t)d;
            std::cerr << std::hex << n << ", ";
            size_t nBytes = PutUVarInt(buf, n);
            CHECK(GetUVarInt(slice(buf, sizeof(buf)), &result) == nBytes);
            CHECK(result == n);
            CHECK(GetUVarInt(slice(buf, nBytes), &result) == nBytes);
            CHECK(result == n);
            CHECK(GetUVarInt(slice(buf, nBytes - 1), &result) == 0);
        }
        std::cerr << "\n";

        // Illegally long number:
        memset(buf, 0x88, sizeof(buf));
        CHECK(GetUVarInt(slice(buf, sizeof(buf)), &result) == 0);
    }

    TEST_CASE("VarInt32 read") {
        uint8_t buf[100];
        for (double d = 0.0; d <= UINT64_MAX; d = std::max(d, 1.0) * 1.5) {
            auto n = (uint64_t)d;
            std::cerr << std::hex << n << ", ";
            size_t nBytes = PutUVarInt(buf, n);
            uint32_t result;
            if (n <= UINT32_MAX) {
                CHECK(GetUVarInt32(slice(buf, sizeof(buf)), &result) == nBytes);
                CHECK(result == n);
                CHECK(GetUVarInt32(slice(buf, nBytes), &result) == nBytes);
                CHECK(result == n);
                CHECK(GetUVarInt32(slice(buf, nBytes - 1), &result) == 0);
            } else {
                CHECK(GetUVarInt32(slice(buf, sizeof(buf)), &result) == 0);
            }
        }
        std::cerr << "\n";
    }

    TEST_CASE("Pointers") {
        ValueTests::testPointers();
    }

    TEST_CASE("Deref") {
        ValueTests::testDeref();
    }

}
