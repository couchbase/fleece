//
// MutableTests.cc
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
#include "Fleece.hh"
#include "MutableArray.hh"
#include "MutableDict.hh"

namespace fleece {

    TEST_CASE("MutableArray attributes", "[Mutable]") {
        MutableArray ma;
        const Value *v = ma.asValuePointer();

        CHECK(MutableArray::isMutableCollection(v));
        CHECK(MutableArray::asMutableArray(v) == &ma);

#if 0
        CHECK(v->type() == kArray);

        CHECK(v->asBool() == true);
        CHECK(v->asInt() == 0);
        CHECK(v->asUnsigned() == 0);
        CHECK(v->asFloat() == 0.0);
        CHECK(v->asDouble() == 0.0);

        CHECK(!v->isInteger());
        CHECK(!v->isUnsigned());
        CHECK(!v->isDouble());

        CHECK(v->asString() == nullslice);
        CHECK(v->asData() == nullslice);
        CHECK(v->toString() == nullslice);
        CHECK(v->asDict() == nullptr);
        CHECK(v->asArray() == &ma);
        CHECK(v->asMutableDict() == nullptr);
        CHECK(v->asMutableArray() == &ma);
#endif
    }


    TEST_CASE("MutableArray set values", "[Mutable]") {
        MutableArray ma;

        REQUIRE(ma.count() == 0);
        //REQUIRE(ma.empty());  //FIX!
        REQUIRE(ma.get(0) == nullptr);

        {
            MutableArray::iterator i(&ma);
            CHECK(!i);
        }

        CHECK(!ma.isChanged());
        ma.resize(9);
        CHECK(ma.isChanged());
        REQUIRE(ma.count() == 9);
        REQUIRE(ma.count() == 9);
        REQUIRE(!ma.empty());

        for (int i = 0; i < 9; i++)
            REQUIRE(ma.get(i)->type() == kNull);

        ma.set(0, nullValue);
        ma.set(1, false);
        ma.set(2, true);
        ma.set(3, 0);
        ma.set(4, -123);
        ma.set(5, 2017);
        ma.set(6, 123456789);
        ma.set(7, -123456789);
        ma.set(8, "Hot dog"_sl);

        static const valueType kExpectedTypes[9] = {
            kNull, kBoolean, kBoolean, kNumber, kNumber, kNumber, kNumber, kNumber, kString};
        for (int i = 0; i < 9; i++)
            CHECK(ma.get(i)->type() == kExpectedTypes[i]);
        CHECK(ma.get(1)->asBool() == false);
        CHECK(ma.get(2)->asBool() == true);
        CHECK(ma.get(3)->asInt() == 0);
        CHECK(ma.get(4)->asInt() == -123);
        CHECK(ma.get(5)->asInt() == 2017);
        CHECK(ma.get(6)->asInt() == 123456789);
        CHECK(ma.get(7)->asInt() == -123456789);
        CHECK(ma.get(8)->asString() == "Hot dog"_sl);

        {
            MutableArray::iterator i(&ma);
            for (int n = 0; n < 9; ++n) {
                std::cerr << "Item " << n << ": " << (void*)i.value() << "\n";
                CHECK(i);
                CHECK(i.value() != nullptr);
                CHECK(i.value()->type() == kExpectedTypes[n]);
                ++i;
            }
            CHECK(!i);
        }

//        CHECK(ma.toJSON() == "[null,false,true,0,-123,2017,123456789,-123456789,\"Hot dog\"]"_sl);

        ma.remove(3, 5);
        CHECK(ma.count() == 4);
        CHECK(ma.get(2)->type() == kBoolean);
        CHECK(ma.get(2)->asBool() == true);
        CHECK(ma.get(3)->type() == kString);

        ma.insert(1, 2);
        CHECK(ma.count() == 6);
        REQUIRE(ma.get(1)->type() == kNull);
        REQUIRE(ma.get(2)->type() == kNull);
        CHECK(ma.get(3)->type() == kBoolean);
        CHECK(ma.get(3)->asBool() == false);
    }


    TEST_CASE("MutableArray pointers", "[Mutable]") {
        MutableArray ma;
        ma.resize(2);
        ma.set(0, 123);
        ma.set(1, 456);

        MutableArray mb;
        CHECK(!mb.isChanged());
        mb.append(&ma);
        CHECK(mb.isChanged());

        CHECK(mb.get(0) == ma.asValuePointer());
        CHECK(mb.makeArrayMutable(0) == &ma);

        Encoder enc;
        enc.beginArray();
        enc << "totoro";
        enc << "catbus";
        enc.endArray();
        alloc_slice data = enc.extractOutput();
        const Array* fleeceArray = Value::fromData(data)->asArray();

//        CHECK(fleeceArray->asMutableArray() == nullptr);

        mb.append(fleeceArray);
        CHECK(mb.get(1) == fleeceArray);
        auto mc = mb.makeArrayMutable(1);
        CHECK(mc != nullptr);
        CHECK(mc->asValuePointer() == mb.get(1));
        CHECK(mb.get(1)->type() == kArray);

        CHECK(mc->count() == 2);
        CHECK(((const Array*)mc)->count() == 2);
        CHECK(mc->get(0)->asString() == "totoro"_sl);
        CHECK(mc->get(1)->asString() == "catbus"_sl);
    }


    TEST_CASE("MutableDict attributes", "[Mutable]") {
        MutableDict md;
        const Value *v = md.asValuePointer();

        CHECK(MutableDict::isMutableCollection(v));
        CHECK(MutableDict::asMutableDict(v) == &md);

#if 0
        CHECK(v->type() == kDict);

        CHECK(v->asBool() == true);
        CHECK(v->asInt() == 0);
        CHECK(v->asUnsigned() == 0);
        CHECK(v->asFloat() == 0.0);
        CHECK(v->asDouble() == 0.0);

        CHECK(!v->isInteger());
        CHECK(!v->isUnsigned());
        CHECK(!v->isDouble());

        CHECK(v->asString() == nullslice);
        CHECK(v->asData() == nullslice);
        CHECK(v->toString() == nullslice);
        CHECK(v->asArray() == nullptr);
        CHECK(v->asDict() == &md);
        CHECK(v->asMutableArray() == nullptr);
        CHECK(v->asMutableDict() == &md);
#endif
    }


    TEST_CASE("MutableDict set values", "[Mutable]") {
        MutableDict md;
        const Dict *d = (const Dict*)md.asValuePointer();

        REQUIRE(md.count() == 0);
        REQUIRE(d->count() == 0);
        //REQUIRE(d->empty());  //FIX!
        CHECK(d->get("foo"_sl) == nullptr);

        {
            MutableDict::iterator i(&md);
            CHECK(!i);
        }
        {
            Dict::iterator i(d);
            CHECK(!i);
        }
        CHECK(!md.isChanged());

        md.set("null"_sl, nullValue);
        md.set("f"_sl, false);
        md.set("t"_sl, true);
        md.set("z"_sl, 0);
        md.set("-"_sl, -123);
        md.set("+"_sl, 2017);
        md.set("hi"_sl, 123456789);
        md.set("lo"_sl, -123456789);
        md.set("str"_sl, "Hot dog"_sl);

        static const slice kExpectedKeys[9] = {
            "+"_sl, "-"_sl, "f"_sl, "hi"_sl, "lo"_sl, "null"_sl, "str"_sl, "t"_sl, "z"_sl};
        static const valueType kExpectedTypes[9] = {
            kNumber, kNumber, kBoolean, kNumber, kNumber, kNull, kString, kBoolean, kNumber};
        for (int i = 0; i < 9; i++)
            REQUIRE(d->get(kExpectedKeys[i])->type() == kExpectedTypes[i]);

        CHECK(d->get("f"_sl)->asBool() == false);
        CHECK(d->get("t"_sl)->asBool() == true);
        CHECK(d->get("z"_sl)->asInt() == 0);
        CHECK(d->get("-"_sl)->asInt() == -123);
        CHECK(d->get("+"_sl)->asInt() == 2017);
        CHECK(d->get("hi"_sl)->asInt() == 123456789);
        CHECK(d->get("lo"_sl)->asInt() == -123456789);
        CHECK(d->get("str"_sl)->asString() == "Hot dog"_sl);
        CHECK(d->get("foo"_sl) == nullptr);

        {
            bool found[9] = { };
            Dict::iterator i(d);
            for (int n = 0; n < 9; ++n) {
                std::cerr << "Item " << n << ": " << i.keyString() << " = " << (void*)i.value() << "\n";
                CHECK(i);
                slice key = i.keyString();
                auto j = std::find(&kExpectedKeys[0], &kExpectedKeys[9], key) - &kExpectedKeys[0];
                REQUIRE(j < 9);
                REQUIRE(found[j] == false);
                found[j] = true;
                CHECK(i.value() != nullptr);
                CHECK(i.value()->type() == kExpectedTypes[j]);
                ++i;
            }
            CHECK(!i);
        }

        {
            Dict::iterator i(d);
            for (int n = 0; n < 9; ++n) {
                std::cerr << "Item " << n << ": " << i.keyString() << " = " << (void*)i.value() << "\n";
                CHECK(i);
                CHECK(i.keyString() == kExpectedKeys[n]);
                CHECK(i.value()->type() == kExpectedTypes[n]);
                ++i;
            }
            CHECK(!i);
        }

        md.remove("lo"_sl);
        CHECK(d->get("lo"_sl) == nullptr);

        CHECK(d->toJSON() == "{\"+\":2017,\"-\":-123,\"f\":false,\"hi\":123456789,\"null\":null,\"str\":\"Hot dog\",\"t\":true,\"z\":0}"_sl);

        md.removeAll();
        CHECK(md.count() == 0);
        {
            MutableDict::iterator i(&md);
            CHECK(!i);
        }
    }


    TEST_CASE("Mutable long strings", "[Mutable]") {
        const char *chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        MutableArray ma(50);
        for (int len = 0; len < 50; ++len)
            ma.set(len, slice(chars, len));
        for (int len = 0; len < 50; ++len)
            CHECK(ma.get(len)->asString() == slice(chars, len));
    }


    TEST_CASE("Encoding mutable array", "[Mutable]") {
        alloc_slice data;
        {
            Encoder enc;
            enc.beginArray();
            enc << "totoro";
            enc << "catbus";
            enc.endArray();
            data = enc.extractOutput();
        }
        std::cerr << "Original data: " << data << "\n";
        const Array* fleeceArray = Value::fromData(data)->asArray();
        std::cerr << "Contents:      " << fleeceArray->toJSON().asString() << "\n";

        Encoder enc2;
        enc2.setBase(data);
        enc2.beginArray();
        enc2 << fleeceArray->get(1);
        enc2 << fleeceArray->get(0);
        enc2.endArray();
        alloc_slice data2 = enc2.extractOutput();
        std::cerr << "Delta:         " << data2 << "\n";
        REQUIRE(data2.size == 8);      // may change slightly with changes to implementation

        data.append(data2);
        const Array* newArray = Value::fromData(data)->asArray();
        std::cerr << "Contents:      " << newArray->toJSON().asString() << "\n";
    }


    TEST_CASE("Encoding mutable dict", "[Mutable]") {
        alloc_slice data;
        {
            Encoder enc;
            enc.beginDictionary();
            enc.writeKey("Name");
            enc << "totoro";
            enc.writeKey("Vehicle");
            enc << "catbus";
            enc.endDictionary();
            data = enc.extractOutput();
        }
        const Dict* originalDict = Value::fromData(data)->asDict();
        std::cerr << "Contents:      " << originalDict->toJSON().asString() << "\n";
        std::cerr << "Original data: " << data << "\n\n";
        Value::dump(data, std::cerr);

        MutableDict update(originalDict);
        CHECK(update.count() == 2);
        update.set("Friend"_sl, "catbus"_sl);
        CHECK(update.count() == 3);
        update.set("Vehicle"_sl, "top"_sl);
        CHECK(update.count() == 3);

        {
            MutableDict::iterator i(&update);
            CHECK(i);
            CHECK(i.keyString() == "Friend"_sl);
            CHECK(i.value()->asString() == "catbus"_sl);
            ++i;
            CHECK(i);
            CHECK(i.keyString() == "Name"_sl);
            CHECK(i.value()->asString() == "totoro"_sl);
            ++i;
            CHECK(i);
            CHECK(i.keyString() == "Vehicle"_sl);
            CHECK(i.value()->asString() == "top"_sl);
            ++i;
            CHECK(!i);
        }

        {
            MutableDict::iterator i(&update);
            CHECK(i.count() == 3);
            CHECK(i);
            CHECK(i.keyString() == "Friend"_sl);
            CHECK(i.value()->asString() == "catbus"_sl);
            ++i;
            CHECK(i);
            CHECK(i.keyString() == "Name"_sl);
            CHECK(i.value()->asString() == "totoro"_sl);
            ++i;
            CHECK(i);
            CHECK(i.keyString() == "Vehicle"_sl);
            CHECK(i.value()->asString() == "top"_sl);
            ++i;
            CHECK(!i);
        }

#if 0
        Encoder enc2;
        enc2.setBase(data);
        enc2.reuseBaseStrings();
        enc2.writeValue(&update);
        alloc_slice data2 = enc2.extractOutput();
        REQUIRE(data2.size == 28);      // may change slightly with changes to implementation

        alloc_slice combinedData(data);
        combinedData.append(data2);
        const Dict* newDict = Value::fromData(combinedData)->asDict();
        std::cerr << "\nContents:      " << newDict->toJSON().asString() << "\n";
        std::cerr << "Delta:         " << data2 << "\n\n";
        Value::dump(combinedData, std::cerr);
#endif

        // Check that removeAll works when there's a base Dict:
        update.removeAll();
        CHECK(update.count() == 0);
        {
            MutableDict::iterator i(&update);
            CHECK(!i);
        }
    }


    TEST_CASE("Larger mutable dict", "[Mutable]") {
        mmap_slice data(kTestFilesDir "1person.fleece");
        auto person = Value::fromTrustedData(data)->asDict();

        std::cerr << "Original data: " << data << "\n";
        std::cerr << "Contents:      " << person->toJSON().asString() << "\n";
        Value::dump(data, std::cerr);

        MutableDict mp(person);
        mp.set("age"_sl, 31);
        MutableArray *friends = mp.makeArrayMutable("friends"_sl);
        auto frend = friends->makeDictMutable(1);
        frend->set("name"_sl, "Reddy Kill-a-Watt"_sl);

#if 0
        Encoder enc;
        enc.setBase(data);
        enc.reuseBaseStrings();
        enc.writeValue(&mp);
        alloc_slice data2 = enc.extractOutput();

        alloc_slice combined(data);
        combined.append(data2);
        const Dict* newDict = Value::fromData(combined)->asDict();
        std::cerr << "\n\nContents:      " << newDict->toJSON().asString() << "\n";
        std::cerr << "Delta:         " << data2 << "\n\n";
        Value::dump(combined, std::cerr);
#endif
    }

}
