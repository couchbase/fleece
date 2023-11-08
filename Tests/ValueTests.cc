//
// ValueTests.cc
//
// Copyright 2015-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "fleece/Fleece.h"
#include "FleeceTests.hh"
#include "Value.hh"
#include "Pointer.hh"
#include "varint.hh"
#include "DeepIterator.hh"
#include "SharedKeys.hh"
#include "Doc.hh"
#include <iostream>
#include <sstream>

#undef NOMINMAX


namespace fleece {
    using namespace std;
    using namespace impl;
    using namespace impl::internal;

    class ValueTests {  // Value declares this as a friend so it can call private API
    public:

        static void testPointers() {
            Pointer v(4, kNarrow);
            REQUIRE(v.offset<false>() == 4u);
            Pointer w(4, kWide);
            REQUIRE(w.offset<true>() == 4u);
        }

        static void testDeref() {
            uint8_t data[10] = {0x01, 0x02, 0x03, 0x04, 0x80, 0x02};
            auto start = (const Pointer*)&data[4];
            REQUIRE(start->offset<false>() == 4u);
            auto dst = start->deref<false>();
            REQUIRE((ptrdiff_t)dst - (ptrdiff_t)&data[0] == 0L);
        }

    };

    TEST_CASE("VarInt read") {
        uint8_t buf[100];
        uint64_t result;
        for (double d = 0.0; d <= UINT64_MAX; d = std::max(d, 1.0) * 1.5) {
            auto n = (uint64_t)d;
            std::cerr << std::hex << n << std::dec << ", ";
            size_t nBytes = PutUVarInt(buf, n);
            REQUIRE(GetUVarInt(slice(buf, sizeof(buf)), &result) == nBytes);
            CHECK(result == n);
            REQUIRE(GetUVarInt(slice(buf, nBytes), &result) == nBytes);
            CHECK(result == n);
            REQUIRE(GetUVarInt(slice(buf, nBytes - 1), &result) == 0);
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
            std::cerr << std::hex << n << std::dec << ", ";
            size_t nBytes = PutUVarInt(buf, n);
            uint32_t result;
            if (n <= UINT32_MAX) {
                REQUIRE(GetUVarInt32(slice(buf, sizeof(buf)), &result) == nBytes);
                CHECK(result == n);
                REQUIRE(GetUVarInt32(slice(buf, nBytes), &result) == nBytes);
                CHECK(result == n);
                REQUIRE(GetUVarInt32(slice(buf, nBytes - 1), &result) == 0);
            } else {
                REQUIRE(GetUVarInt32(slice(buf, sizeof(buf)), &result) == 0);
            }
        }
        std::cerr << "\n";
    }

    TEST_CASE("Constants") {
        CHECK(Value::kNullValue->type() == kNull);
        CHECK(!Value::kNullValue->isUndefined());
        CHECK(!Value::kNullValue->isMutable());             // tests even-address alignment

        CHECK(Value::kUndefinedValue->type() == kNull);
        CHECK(Value::kUndefinedValue->isUndefined());
        CHECK(!Value::kUndefinedValue->isMutable());

        CHECK(Array::kEmpty->type() == kArray);
        CHECK(Array::kEmpty->count() == 0);
        CHECK(!Array::kEmpty->isMutable());

        CHECK(Dict::kEmpty->type() == kDict);
        CHECK(Dict::kEmpty->count() == 0);
        CHECK(!Dict::kEmpty->isMutable());
    }

    TEST_CASE("Pointers") {
        fleece::ValueTests::testPointers();
    }

    TEST_CASE("Deref") {
        fleece::ValueTests::testDeref();
    }

    TEST_CASE("DeepIterator") {
        auto input = readTestFile("1person.fleece");
        auto person = Value::fromData(input);

        {
            // Check iterating null:
            DeepIterator i(nullptr);
            CHECK(i.value() == nullptr);
            CHECK(!i);
            i.next();
        }

        {
            // Check iterating a non-collection (a string in this case):
            auto str = person->asDict()->get("_id"_sl);
            CHECK(str->type() == kString);
            DeepIterator i(str);
            CHECK(i);
            CHECK(i.value() == str);
            CHECK(i.keyString() == nullslice);
            CHECK(i.index() == 0);
            CHECK(i.path().size() == 0);
            i.next();
            CHECK(!i);
        }

        {
            stringstream s;
            for (DeepIterator i(person); i; ++i) {
                s << i.jsonPointer() << ": " << i.value()->toString().asString() << "\n";
            }
            //cerr << s.str();
#if FL_HAVE_TEST_FILES
            CHECK(s.str() == readFile(kTestFilesDir "1person-deepIterOutput.txt").asString());
#endif
        }

        {
            stringstream s;
            for (DeepIterator i(person); i; ++i) {
                if (i.path().empty())
                    continue;
                s << i.jsonPointer() << ": " << i.value()->toString().asString() << "\n";
                i.skipChildren();
            }
            //cerr << s.str();
#if FL_HAVE_TEST_FILES
            CHECK(s.str() == readFile(kTestFilesDir "1person-shallowIterOutput.txt").asString());
#endif
        }
    }


    TEST_CASE("Doc", "[SharedKeys]") {
        const Dict *root;
        {
            Retained<SharedKeys> sk = new SharedKeys();
            Retained<Doc> doc = new Doc(readTestFile("1person.fleece"), Doc::kUntrusted, sk);
            CHECK(doc->sharedKeys() == sk);
            root = (const Dict*)doc->root();
            CHECK(root);
            CHECK(Doc::sharedKeys(root) == sk);

            auto id = root->get("_id"_sl);
            REQUIRE(id);
            CHECK(Doc::sharedKeys(id) == sk);
        }
        CHECK(Doc::sharedKeys(root) == nullptr);
    }


    TEST_CASE("Duplicate Docs", "[SharedKeys]") {
        const Dict *root;
        {
            alloc_slice data( readTestFile("1person.fleece") );
            Retained<SharedKeys> sk = new SharedKeys();
            Retained<Doc> doc1 = new Doc(data, Doc::kUntrusted, sk);
            Retained<Doc> doc2 = new Doc(data, Doc::kUntrusted, sk);
            CHECK(doc1->data() == data);
            CHECK(doc2->data() == data);
            CHECK(doc1->sharedKeys() == sk);
            CHECK(doc2->sharedKeys() == sk);
            root = (const Dict*)doc1->root();
            CHECK(root);
            CHECK(root->sharedKeys() == sk);

            root = (const Dict*)doc2->root();
            CHECK(root);
            CHECK(root->sharedKeys() == sk);
        }
        CHECK(Doc::sharedKeys(root) == nullptr);
    }

    TEST_CASE("Retain empty array contained in Doc", "[Doc]") {
        // https://github.com/couchbaselabs/fleece/issues/113
        Retained<Doc> doc = Doc::fromJSON("[]");
        auto root = doc->root();
        retain(root);
        release(root);
    }

    TEST_CASE("Recreate Doc from same data", "[Doc]") {
        alloc_slice data( readTestFile("1person.fleece") );
        Retained<Doc> doc = new Doc(data, Doc::kUntrusted);
        doc = nullptr;
        doc = new Doc(data, Doc::kUntrusted);
    }

    TEST_CASE("Many Docs", "[Doc]") {
        std::vector<Retained<Doc>> docs;
        for (int i = 0; i < 100; i++) {
            docs.push_back(Doc::fromJSON("[]"));
        }
    }

    TEST_CASE("Array Iterators") {
        FLDoc doc = nullptr;
        SECTION("Empty Array") {
            doc = FLDoc_FromJSON("[]"_sl, nullptr);
        }
        SECTION("Non Empty Array") {
            doc = FLDoc_FromJSON("[1]"_sl, nullptr);
        }
        REQUIRE(doc);

        FLValue val = FLDoc_GetRoot(doc);
        FLArray arr = FLValue_AsArray(val);
        REQUIRE(arr);
        Array::iterator iter = Array::iterator((Array*)arr);

        bool caughtException = false;
        bool capturedBacktrace = true;

        // No exception with typical loop.
        try {
            for (; iter; ++iter);
        } catch (...) {
            caughtException = true;
        }
        CHECK(!caughtException);

        caughtException = false;
        // ++iter will throw if already at the end.
        // OutOfRange exception should contain the backtrace.
        try {
            ++iter;
        } catch (const FleeceException& exc) {
            if (exc.code == (int)kFLOutOfRange) {
                caughtException = true;
                capturedBacktrace = !!exc.backtrace;
            }
        }
        CHECK((caughtException && !capturedBacktrace));

        // FL Itarator
        FLArrayIterator flIter;
        FLArrayIterator_Begin(arr, &flIter);
        FLValue value;
        while (NULL != (value = FLArrayIterator_GetValue(&flIter))) {
            FLArrayIterator_Next(&flIter);
        }
        // Calling Next is okay. It will trigger exception but we won't try to capture the backtrace.
        CHECK(!FLArrayIterator_Next(&flIter));

        FLDoc_Release(doc);
    }

    TEST_CASE("Dict Iterators") {
        FLDoc doc = nullptr;
        SECTION("Empty Dict") {
            doc = FLDoc_FromJSON("{}"_sl, nullptr);
        }
        SECTION("Non Empty Dict") {
            doc = FLDoc_FromJSON(R"({"key": 1})"_sl, nullptr);
        }
        REQUIRE(doc);

        FLValue val = FLDoc_GetRoot(doc);
        FLDict dict = FLValue_AsDict(val);
        REQUIRE(dict);
        Dict::iterator iter = Dict::iterator((Dict*)dict);

        bool caughtException = false;
        bool capturedBacktrace = true;

        // No exception with typical loop pattern.
        try {
            for (; iter; ++iter);
        } catch (...) {
            caughtException = true;
        }
        CHECK(!caughtException);

        caughtException = false;
        // ++iter will throw if already at the end.
        // OutOfRange exception should contain the backtrace.
        try {
            ++iter;
        } catch (const FleeceException& exc) {
            if (exc.code == (int)kFLOutOfRange) {
                caughtException = true;
                capturedBacktrace = !!exc.backtrace;
            }
        }
        CHECK((caughtException && !capturedBacktrace));

        // FL Itarator
        FLDictIterator flIter;
        FLDictIterator_Begin(dict, &flIter);
        FLValue value;
        while (NULL != (value = FLDictIterator_GetValue(&flIter))) {
            FLDictIterator_Next(&flIter);
        }
        // Calling Next is okay. It will trigger exception but we won't try to capture the backtrace.
        // Cannot assert it directly. Internally, it uses Dict::iterator::operator++().
        CHECK(!FLDictIterator_Next(&flIter));

        FLDoc_Release(doc);
    }
}
