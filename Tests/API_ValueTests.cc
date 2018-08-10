//
// API_ValueTests.cc
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
#include "fleece/Fleece.hh"

using namespace fleece;


#if 0
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
#endif


TEST_CASE("API Doc") {
    Dict root;
    {
        SharedKeys sk = SharedKeys::create();
        Doc doc(readTestFile("1person.fleece"), kFLUntrusted, sk);
        CHECK(doc.sharedKeys() == sk);
        root = doc.root().asDict();
        CHECK(root);
        CHECK(root.findDoc() == doc);

        auto id = root.get("_id"_sl);
        REQUIRE(id);
        CHECK(id.findDoc() == doc);
    }
    CHECK(!root.findDoc());
}


TEST_CASE("API Paths", "[Encoder]") {
    Doc doc = Doc::fromJSON(readTestFile(kBigJSONTestFileName));
    auto root = doc.root();
    CHECK(root.asArray().count() == kBigJSONTestCount);

    FLError error;
    KeyPath p1{"$[32].name"_sl, &error};
    Value name = root[p1];
    REQUIRE(name);
    REQUIRE(name.type() == kFLString);
    REQUIRE(name.asString() == "Mendez Tran"_sl);

    KeyPath p2{"[-1].name"_sl, &error};
    name = root[p2];
    REQUIRE(name);
    REQUIRE(name.type() == kFLString);
#if FL_HAVE_TEST_FILES
    REQUIRE(name.asString() == "Marva Morse"_sl);
#else  // embedded test uses only 50 people, not 1000, so [-1] resolves differently
    REQUIRE(name.asString() == slice("Tara Wall"));
#endif
}

