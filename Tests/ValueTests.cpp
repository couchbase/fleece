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

    TEST_CASE("Pointers") {
        ValueTests::testPointers();
    }

    TEST_CASE("Deref") {
        ValueTests::testDeref();
    }

}
