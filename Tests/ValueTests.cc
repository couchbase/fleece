//
// ValueTests.cc
//
// Copyright (c) 2015 Couchbase, Inc All rights reserved.
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

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "FleeceTests.hh"
#include "Value.hh"
#include "Pointer.hh"
#include "varint.hh"
#include "DeepIterator.hh"
#include "SharedKeys.hh"
#include "Doc.hh"
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
            uint8_t data[6] = {0x01, 0x02, 0x03, 0x04, 0x80, 0x02};
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
            std::cerr << std::hex << n << std::dec << ", ";
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

}
