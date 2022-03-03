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
        for (double d = 0.0; d <= (double)UINT64_MAX; d = std::max(d, 1.0) * 1.5) {
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
        for (double d = 0.0; d <= (double)UINT64_MAX; d = std::max(d, 1.0) * 1.5) {
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

    TEST_CASE("Empty FLArrayIterator", "[API]") {
        FLDoc doc = FLDoc_FromJSON("[]"_sl, nullptr);
        FLArray arr = FLValue_AsArray(FLDoc_GetRoot(doc));
        REQUIRE(arr);
        FLArrayIterator iter;
        FLArrayIterator_Begin(arr, &iter);
        CHECK(FLArrayIterator_GetValue(&iter) == nullptr);
        CHECK(FLArrayIterator_GetCount(&iter) == 0);
        // (calling FLArrayIterator_Next would be illegal)
        FLDoc_Release(doc);
    }

    TEST_CASE("Empty FLDictIterator", "[API]") {
        FLDoc doc = FLDoc_FromJSON("{}"_sl, nullptr);
        FLDict arr = FLValue_AsDict(FLDoc_GetRoot(doc));
        REQUIRE(arr);
        FLDictIterator iter;
        FLDictIterator_Begin(arr, &iter);
        CHECK(FLDictIterator_GetValue(&iter) == nullptr);
        CHECK(FLDictIterator_GetKey(&iter) == nullptr);
        CHECK(FLDictIterator_GetKeyString(&iter) == kFLSliceNull);
        CHECK(FLDictIterator_GetCount(&iter) == 0);
        // (calling FLDictIterator_Next would be illegal)
        FLDoc_Release(doc);
    }

    TEST_CASE("Many Levels in a Doc", "[Doc]") {
        auto genDoc = [](unsigned nlevels) -> string {
            // pre-condition: nlevels >= 1
            string ret = "";
            for (unsigned i = 1; i <= nlevels; ++i) {
                ret += R"({"a":)";
            }
            ret += "1}";
            for (unsigned i = 2; i <= nlevels; ++i) {
                ret += "}";
            }
            return ret;
        };

        SECTION("100 Levels of JSON Dictionary") {
            alloc_slice origJSON{genDoc(100)};
            Retained<Doc> doc = Doc::fromJSON(origJSON);
            auto root = doc->root();
            alloc_slice json = root->toJSON();
            CHECK(origJSON == json);
            retain(root);
            release(root);
        }

        SECTION("100 Is the Limit of JSON Dictionary") {
            alloc_slice origJSON{genDoc(101)};
            Retained<Doc> doc;
            fleece::ErrorCode errCode = fleece::NoError;
            try {
                doc = Doc::fromJSON(origJSON);
            } catch (FleeceException& exc) {
                errCode = fleece::JSONError;
            }
            CHECK(errCode == fleece::JSONError);
        }
    }
}
