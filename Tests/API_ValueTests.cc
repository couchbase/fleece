//
// API_ValueTests.cc
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "fleece/Fleece.hh"
#include <iostream>

namespace fleece {
    static inline std::ostream& operator<<(std::ostream &out, const fleece::Doc &doc) {
        out << "Doc(" << (void*)(FLDoc)doc << ")";
        return out;
    }
}

#include "FleeceTests.hh"
#include "fleece/Fleece.hh"
#include "fleece/Mutable.hh"

using namespace fleece;
using namespace std;


#if 0
TEST_CASE("DeepIterator", "[API]") {
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


TEST_CASE("API Doc", "[API][SharedKeys]") {
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


TEST_CASE("API Encoder", "[API][Encoder]") {
    Encoder enc;
    enc.beginDict();
    enc["foo"_sl] = 17;
    enc["bar"_sl] = "wow";
    enc["bool"_sl] = true;
//    enc["huh?"_sl] = &enc;
    enc.endDict();
    Doc doc = enc.finishDoc();

    CHECK(doc["foo"_sl].asInt() == 17);
    CHECK(doc["bar"_sl].asString() == "wow"_sl);
    CHECK(doc["bool"_sl].asBool() == true);
    CHECK(doc["bool"_sl].type() == kFLBoolean);
}


TEST_CASE("API Paths", "[API][Encoder]") {
    alloc_slice fleeceData = readTestFile(kBigJSONTestFileName);
    Doc doc = Doc::fromJSON(fleeceData);
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


TEST_CASE("API Undefined", "[API]") {
    Encoder enc;
    enc.beginArray();
    enc.writeInt(1234);
    enc.writeUndefined();
    enc.writeInt(4321);
    enc.endArray();
    auto doc = enc.finishDoc();

    Array a = doc.root().asArray();
    CHECK(a[0].asInt() == 1234);
    CHECK(a[1].type() == kFLUndefined);
    CHECK(a[2].asInt() == 4321);

    Array::iterator i(a);
    CHECK(i);
    CHECK(i.value().asInt() == 1234);
    ++i;
    CHECK(i);
    CHECK(i.value().type() == kFLUndefined);
    ++i;
    CHECK(i);
    CHECK(i.value().asInt() == 4321);
    ++i;
    CHECK(!i);
}


TEST_CASE("API constants", "[API]") {
    CHECK(Value::null() != nullptr);
    CHECK(Value::null().type() == kFLNull);
    
    CHECK(Value::undefined() != nullptr);
    CHECK(Value::undefined().type() == kFLUndefined);

    CHECK((FLValue)Array::emptyArray() != nullptr);
    CHECK(Array::emptyArray().type() == kFLArray);
    CHECK(Array::emptyArray().count() == 0);

    CHECK((FLValue)Dict::emptyDict() != nullptr);
    CHECK(Dict::emptyDict().type() == kFLDict);
    CHECK(Dict::emptyDict().count() == 0);
}


static MutableArray returnsMutableArray() {
    MutableArray ma = MutableArray::newArray();
    ma.append(17);
    return ma;
}

static MutableDict returnsMutableDict() {
    MutableDict md = MutableDict::newDict();
    md.set("foo"_sl, "bar");
    return md;
}


TEST_CASE("API Mutable Invalid Assignment", "[API]") {
    // This next line should fail to compile:
    //Array a = returnsMutableArray();

    // This does compile, since RetainedValue keeps a reference:
    RetainedValue b = returnsMutableArray();
    CHECK(b.toJSONString() == "[17]");
}

TEST_CASE("API MutableArray", "[API]") {
    MutableArray d = MutableArray::newArray();
    d.append("bar");
    CHECK(d.toJSONString() == "[\"bar\"]");
    REQUIRE(d.count() == 1);
    CHECK(d.get(0).asString() == "bar"_sl);

    d.set(0) = 1234;
    CHECK(d.toJSONString() == "[1234]");
    REQUIRE(d.count() == 1);
    CHECK(d.get(0).asInt() == 1234);

    d[0] = 1234;
    CHECK(d.toJSONString() == "[1234]");
    d[0] = false;
    CHECK(d.toJSONString() == "[false]");
    d[0] = "hi";
    CHECK(d.toJSONString() == "[\"hi\"]");
    d[0].setNull();
    CHECK(d.toJSONString() == "[null]");
}

TEST_CASE("API MutableDict", "[API]") {
    MutableDict d = MutableDict::newDict();
    d.set("foo"_sl, "bar");
    REQUIRE(d.get("foo"_sl));
    CHECK(d.get("foo"_sl).asString() == "bar"_sl);
    CHECK(d.count() == 1);

    d.set("x"_sl) = 1234;
    CHECK(d.count() == 2);
    CHECK(d.toJSONString() == "{\"foo\":\"bar\",\"x\":1234}");
    REQUIRE(d.get("x"_sl));
    CHECK(d.get("x"_sl).asInt() == 1234);
}

TEST_CASE("API RetainedArray", "[API]") {
    // RetainedArray():
    RetainedArray ra = RetainedArray();
    CHECK(!(FLArray)ra);
    CHECK(ra.count() == 0);
    
    // RetainedArray(FLArray v):
    MutableArray a1 = MutableArray::newArray();
    a1.append("bar1");
    RetainedArray ra1((FLArray)a1);
    a1 = nullptr;
    REQUIRE(ra1.count() == 1);
    CHECK(ra1.get(0).asString() == "bar1"_sl);
    
    // RetainedArray(const Array &v):
    MutableArray a2 = MutableArray::newArray();
    a2.append("bar1");
    RetainedArray ra2 = a2;
    a2 = nullptr;
    REQUIRE(ra2.count() == 1);
    CHECK(ra2.get(0).asString() == "bar1"_sl);
    
    // RetainedArray(RetainedArray &&v):
    RetainedArray ra3 = std::move(ra2);
    REQUIRE(ra2.count() == 0);      // peeking in moved-from object to verify it's empty
    REQUIRE(ra3.count() == 1);
    CHECK(ra3.get(0).asString() == "bar1"_sl);
    
    // RetainedArray(const RetainedArray &v):
    RetainedArray ra4 = ra3;
    ra3 = nullptr;
    REQUIRE(ra3.count() == 0);
    REQUIRE(ra4.count() == 1);
    CHECK(ra4.get(0).asString() == "bar1"_sl);
    
    // RetainedArray(MutableArray &&v):
    RetainedArray ra5 = returnsMutableArray();
    REQUIRE(ra5.count() == 1);
    CHECK(ra5.get(0).asInt() == 17);
    
    // RetainedArray& operator= (const Array &v):
    ra5 = returnsMutableArray();
    REQUIRE(ra5.count() == 1);
    CHECK(ra5.get(0).asInt() == 17);
    
    // RetainedArray& operator= (RetainedArray &&v):
    ra4 = std::move(ra5);
    REQUIRE(ra4.count() == 1);
    CHECK(ra4.get(0).asInt() == 17);
}

TEST_CASE("API RetainedDict", "[API]") {
    // RetainedDict():
    RetainedDict rd = RetainedDict();
    CHECK(!(FLDict)rd);
    CHECK(rd.count() == 0);
    
    // RetainedDict(FLDict v):
    MutableDict d1 = MutableDict::newDict();
    d1.set("foo"_sl, "bar1");
    RetainedDict rd1((FLDict)d1);
    d1 = nullptr;
    REQUIRE(rd1.get("foo"_sl));
    CHECK(rd1.get("foo"_sl).asString() == "bar1"_sl);
    
    // RetainedDict(const Dict &v):
    MutableDict d2 = MutableDict::newDict();
    d2.set("foo"_sl, "bar1");
    RetainedDict rd2 = d2;
    d2 = nullptr;
    REQUIRE(rd2.get("foo"_sl));
    CHECK(rd2.get("foo"_sl).asString() == "bar1"_sl);
    
    // RetainedDict(RetainedDict &&v):
    RetainedDict rd3 = std::move(rd2);
    REQUIRE(rd2.count() == 0);      // peeking in moved-from object to verify it's empty
    REQUIRE(rd3.get("foo"_sl));
    CHECK(rd3.get("foo"_sl).asString() == "bar1"_sl);
    
    // RetainedDict(const RetainedDict &v):
    RetainedDict rd4 = rd3;
    rd3 = nullptr;
    REQUIRE(rd3.count() == 0);
    REQUIRE(rd4.get("foo"_sl));
    CHECK(rd4.get("foo"_sl).asString() == "bar1"_sl);
    
    // RetainedDict(MutableDict &&v):
    RetainedDict rd5 = returnsMutableDict();
    REQUIRE(rd5.get("foo"_sl));
    CHECK(rd5.get("foo"_sl).asString() == "bar"_sl);
    
    // RetainedDict& operator= (const Dict &v):
    rd5 = returnsMutableDict();
    REQUIRE(rd5.get("foo"_sl));
    CHECK(rd5.get("foo"_sl).asString() == "bar"_sl);
    
    // RetainedDict& operator= (RetainedDict &&v):
    rd4 = std::move(rd5);
    REQUIRE(rd4.get("foo"_sl));
    CHECK(rd4.get("foo"_sl).asString() == "bar"_sl);
}


TEST_CASE("API MutableDict item bool conversion", "[API]") {
    // From #105
    MutableDict dict { MutableDict::newDict() };
    dict["a_key"] = 6;

    // This line converted `dict` to `keyref` and thence to `FLSlot`, which was nonnull,
    // causing the test to pass. I've decided the FLSlot conversion operator is unnecessary;
    // removing it fixed the issue.
    if (dict["a_non_existent_key"]) {
        FAIL("Key test failed");
    }
    if (!dict["a_key"]) {
        FAIL("Negative key test failed");
    }
    CHECK(dict.toJSONString() == "{\"a_key\":6}");
}
