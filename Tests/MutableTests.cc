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

    TEST_CASE("MutableArray type checking", "[Mutable]") {
        Retained<MutableArray> ma = MutableArray::newArray();

        CHECK(ma->asArray() == ma);
        CHECK(ma->isMutable());
        CHECK(ma->asMutable() == ma);

        CHECK(ma->type() == kArray);

        CHECK(ma->asBool() == true);
        CHECK(ma->asInt() == 0);
        CHECK(ma->asUnsigned() == 0);
        CHECK(ma->asFloat() == 0.0);
        CHECK(ma->asDouble() == 0.0);

        CHECK(!ma->isInteger());
        CHECK(!ma->isUnsigned());
        CHECK(!ma->isDouble());

        CHECK(ma->asString() == nullslice);
        CHECK(ma->asData() == nullslice);
        CHECK(ma->toString() == nullslice);
        CHECK(ma->asDict() == nullptr);
        CHECK(ma->asArray() == ma);
    }


    TEST_CASE("MutableArray set values", "[Mutable]") {
        Retained<MutableArray> ma = MutableArray::newArray();

        REQUIRE(ma->count() == 0);
        REQUIRE(ma->empty());
        REQUIRE(ma->get(0) == nullptr);

        {
            MutableArray::iterator i(ma);
            CHECK(!i);
        }

        CHECK(!ma->isChanged());
        ma->resize(9);
        CHECK(ma->isChanged());
        REQUIRE(ma->count() == 9);
        REQUIRE(ma->count() == 9);
        REQUIRE(!ma->empty());

        for (int i = 0; i < 9; i++)
            REQUIRE(ma->get(i)->type() == kNull);

        ma->set(0, nullValue);
        ma->set(1, false);
        ma->set(2, true);
        ma->set(3, 0);
        ma->set(4, -123);
        ma->set(5, 2017);
        ma->set(6, 123456789);
        ma->set(7, -123456789);
        ma->set(8, "Hot dog"_sl);

        static const valueType kExpectedTypes[9] = {
            kNull, kBoolean, kBoolean, kNumber, kNumber, kNumber, kNumber, kNumber, kString};
        for (int i = 0; i < 9; i++)
            CHECK(ma->get(i)->type() == kExpectedTypes[i]);
        CHECK(ma->get(1)->asBool() == false);
        CHECK(ma->get(2)->asBool() == true);
        CHECK(ma->get(3)->asInt() == 0);
        CHECK(ma->get(4)->asInt() == -123);
        CHECK(ma->get(5)->asInt() == 2017);
        CHECK(ma->get(6)->asInt() == 123456789);
        CHECK(ma->get(7)->asInt() == -123456789);
        CHECK(ma->get(8)->asString() == "Hot dog"_sl);

        {
            MutableArray::iterator i(ma);
            for (int n = 0; n < 9; ++n) {
                std::cerr << "Item " << n << ": " << (void*)i.value() << "\n";
                CHECK(i);
                CHECK(i.value() != nullptr);
                CHECK(i.value()->type() == kExpectedTypes[n]);
                ++i;
            }
            CHECK(!i);
        }

        CHECK(ma->asArray()->toJSON() == "[null,false,true,0,-123,2017,123456789,-123456789,\"Hot dog\"]"_sl);

        ma->remove(3, 5);
        CHECK(ma->count() == 4);
        CHECK(ma->get(2)->type() == kBoolean);
        CHECK(ma->get(2)->asBool() == true);
        CHECK(ma->get(3)->type() == kString);

        ma->insert(1, 2);
        CHECK(ma->count() == 6);
        REQUIRE(ma->get(1)->type() == kNull);
        REQUIRE(ma->get(2)->type() == kNull);
        CHECK(ma->get(3)->type() == kBoolean);
        CHECK(ma->get(3)->asBool() == false);
    }


    TEST_CASE("MutableArray as Array", "[Mutable]") {
        Retained<MutableArray> ma = MutableArray::newArray();
        const Array *a = ma->asArray();
        CHECK(a->type() == kArray);
        CHECK(a->count() == 0);
        CHECK(a->empty());

        ma->resize(2);
        ma->set(0, 123);
        ma->set(1, 456);

        CHECK(a->count() == 2);
        CHECK(!a->empty());
        CHECK(a->get(0)->asInt() == 123);
        CHECK(a->get(1)->asInt() == 456);

        Array::iterator i(a);
        CHECK(i);
        CHECK(i.value()->asInt() == 123);
        ++i;
        CHECK(i);
        CHECK(i.value()->asInt() == 456);
        ++i;
        CHECK(!i);
    }


    TEST_CASE("MutableArray pointers", "[Mutable]") {
        Retained<MutableArray> ma = MutableArray::newArray();
        ma->resize(2);
        ma->set(0, 123);
        ma->set(1, 456);

        Retained<MutableArray> mb = MutableArray::newArray();
        CHECK(!mb->isChanged());
        mb->append(ma);
        CHECK(mb->isChanged());

        CHECK(mb->get(0) == ma);
        CHECK(mb->getMutableArray(0) == ma);

        Encoder enc;
        enc.beginArray();
        enc << "totoro";
        enc << "catbus";
        enc.endArray();
        alloc_slice data = enc.extractOutput();
        const Array* fleeceArray = Value::fromData(data)->asArray();

        CHECK(fleeceArray->asMutable() == nullptr);

        mb->append(fleeceArray);
        CHECK(mb->get(1) == fleeceArray);
        auto mc = mb->getMutableArray(1);
        CHECK(mc != nullptr);
        CHECK(mc == mb->get(1));
        CHECK(mb->get(1)->type() == kArray);

        CHECK(mc->count() == 2);
        CHECK(mc->asArray()->count() == 2);
        CHECK(mc->get(0)->asString() == "totoro"_sl);
        CHECK(mc->get(1)->asString() == "catbus"_sl);
    }


#pragma mark - MUTABLE DICT:


    TEST_CASE("MutableDict type checking", "[Mutable]") {
        Retained<MutableDict> md = MutableDict::newDict();
        const Dict *d = md;
        CHECK(d->type() == kDict);

        CHECK(d->isMutable());
        CHECK(d->asMutable() == md);

        CHECK(d->type() == kDict);

        CHECK(d->asBool() == true);
        CHECK(d->asInt() == 0);
        CHECK(d->asUnsigned() == 0);
        CHECK(d->asFloat() == 0.0);
        CHECK(d->asDouble() == 0.0);

        CHECK(!d->isInteger());
        CHECK(!d->isUnsigned());
        CHECK(!d->isDouble());

        CHECK(d->asString() == nullslice);
        CHECK(d->asData() == nullslice);
        CHECK(d->toString() == nullslice);
        CHECK(d->asArray() == nullptr);
        CHECK(d->asDict() == d);
    }


    TEST_CASE("MutableDict set values", "[Mutable]") {
        Retained<MutableDict> md = MutableDict::newDict();
        CHECK(md->count() == 0);
        CHECK(md->get("foo"_sl) == nullptr);

        {
            MutableDict::iterator i(md);
            CHECK(!i);
        }

        CHECK(!md->isChanged());

        md->set("null"_sl, nullValue);
        md->set("f"_sl, false);
        md->set("t"_sl, true);
        md->set("z"_sl, 0);
        md->set("-"_sl, -123);
        md->set("+"_sl, 2017);
        md->set("hi"_sl, 123456789);
        md->set("lo"_sl, -123456789);
        md->set("str"_sl, "Hot dog"_sl);
        CHECK(md->count() == 9);

        static const slice kExpectedKeys[9] = {
            "+"_sl, "-"_sl, "f"_sl, "hi"_sl, "lo"_sl, "null"_sl, "str"_sl, "t"_sl, "z"_sl};
        static const valueType kExpectedTypes[9] = {
            kNumber, kNumber, kBoolean, kNumber, kNumber, kNull, kString, kBoolean, kNumber};
        for (int i = 0; i < 9; i++)
            REQUIRE(md->get(kExpectedKeys[i])->type() == kExpectedTypes[i]);

        CHECK(md->get("f"_sl)->asBool() == false);
        CHECK(md->get("t"_sl)->asBool() == true);
        CHECK(md->get("z"_sl)->asInt() == 0);
        CHECK(md->get("-"_sl)->asInt() == -123);
        CHECK(md->get("+"_sl)->asInt() == 2017);
        CHECK(md->get("hi"_sl)->asInt() == 123456789);
        CHECK(md->get("lo"_sl)->asInt() == -123456789);
        CHECK(md->get("str"_sl)->asString() == "Hot dog"_sl);
        CHECK(md->get("foo"_sl) == nullptr);

        {
            bool found[9] = { };
            MutableDict::iterator i(md);
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

        md->remove("lo"_sl);
        CHECK(md->get("lo"_sl) == nullptr);
        CHECK(md->count() == 8);

//        CHECK(md->toJSON() == "{\"+\":2017,\"-\":-123,\"f\":false,\"hi\":123456789,\"null\":null,\"str\":\"Hot dog\",\"t\":true,\"z\":0}"_sl);

        md->removeAll();
        CHECK(md->count() == 0);
        {
            MutableDict::iterator i(md);
            CHECK(!i);
        }
    }


    TEST_CASE("MutableDict as Dict", "[Mutable]") {
        Retained<MutableDict> md = MutableDict::newDict();
        const Dict *d = md;
        CHECK(d->type() == kDict);
        CHECK(d->count() == 0);
        CHECK(d->empty());
        CHECK(d->get("foo"_sl) == nullptr);
        {
            Dict::iterator i(d);
            CHECK(!i);
        }

        md->set("null"_sl, nullValue);
        md->set("f"_sl, false);
        md->set("t"_sl, true);
        md->set("z"_sl, 0);
        md->set("-"_sl, -123);
        md->set("+"_sl, 2017);
        md->set("hi"_sl, 123456789);
        md->set("lo"_sl, -123456789);
        md->set("str"_sl, "Hot dog"_sl);

        static const slice kExpectedKeys[9] = {
            "+"_sl, "-"_sl, "f"_sl, "hi"_sl, "lo"_sl, "null"_sl, "str"_sl, "t"_sl, "z"_sl};
        static const valueType kExpectedTypes[9] = {
            kNumber, kNumber, kBoolean, kNumber, kNumber, kNull, kString, kBoolean, kNumber};
        for (int i = 0; i < 9; i++)
            REQUIRE(d->get(kExpectedKeys[i])->type() == kExpectedTypes[i]);

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

        md->remove("lo"_sl);
        CHECK(d->get("lo"_sl) == nullptr);

        CHECK(d->toJSON() == "{\"+\":2017,\"-\":-123,\"f\":false,\"hi\":123456789,\"null\":null,\"str\":\"Hot dog\",\"t\":true,\"z\":0}"_sl);

        md->removeAll();
        CHECK(d->count() == 0);
        {
            Dict::iterator i(d);
            CHECK(!i);
        }

    }


    TEST_CASE("Mutable long strings", "[Mutable]") {
        const char *chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        Retained<MutableArray> ma = MutableArray::newArray(50);
        for (int len = 0; len < 50; ++len)
            ma->set(len, slice(chars, len));
        for (int len = 0; len < 50; ++len)
            CHECK(ma->get(len)->asString() == slice(chars, len));
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


    template <class ITER>
    static void CHECKiter(ITER &i, const char *key, const char *value) {
        CHECK(i);
        CHECK(i.keyString() == slice(key));
        CHECK(i.value()->asString() == slice(value));
        ++i;
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
            enc.writeKey("Mood");
            enc << "happy";
            enc.writeKey("Asleep");
            enc << "true";
            enc.writeKey("Size");
            enc << "XXXL";
            enc.endDictionary();
            data = enc.extractOutput();
        }
        const Dict* originalDict = Value::fromData(data)->asDict();
        std::cerr << "Contents:      " << originalDict->toJSON().asString() << "\n";
        std::cerr << "Original data: " << data << "\n\n";
        Value::dump(data, std::cerr);

        Retained<MutableDict> update = MutableDict::newDict(originalDict);
        CHECK(update->count() == 5);
        update->set("Friend"_sl, "catbus"_sl);
        CHECK(update->count() == 6);
        update->set("Vehicle"_sl, "top"_sl);
        CHECK(update->count() == 6);
        update->remove("Asleep"_sl);
        CHECK(update->count() == 5);
        update->remove("Asleep"_sl);
        CHECK(update->count() == 5);
        update->remove("Q"_sl);
        CHECK(update->count() == 5);

        {
            MutableDict::iterator i(update);
            CHECKiter(i, "Friend", "catbus");
            CHECKiter(i, "Mood", "happy");
            CHECKiter(i, "Name", "totoro");
            CHECKiter(i, "Size", "XXXL");
            CHECKiter(i, "Vehicle", "top");
            CHECK(!i);
        }

        { // Try the same thing but with a Dict iterator:
            Dict::iterator i(update);
            CHECKiter(i, "Friend", "catbus");
            CHECKiter(i, "Mood", "happy");
            CHECKiter(i, "Name", "totoro");
            CHECKiter(i, "Size", "XXXL");
            CHECKiter(i, "Vehicle", "top");
            CHECK(!i);
        }

        Encoder enc2;
        enc2.setBase(data);
        enc2.reuseBaseStrings();
        enc2.writeValue(update);
        alloc_slice delta = enc2.extractOutput();
        REQUIRE(delta.size == 32);      // may change slightly with changes to implementation

        // Check that removeAll works when there's a base Dict:
        update->removeAll();
        CHECK(update->count() == 0);
        {
            MutableDict::iterator i(update);
            CHECK(!i);
        }

        alloc_slice combinedData(data);
        combinedData.append(delta);
        const Dict* newDict = Value::fromData(combinedData)->asDict();
        std::cerr << "Delta:         " << delta << "\n\n";
        Value::dump(combinedData, std::cerr);

        CHECK(newDict->get("Name"_sl)->asString() == "totoro"_sl);
        CHECK(newDict->get("Friend"_sl)->asString() == "catbus"_sl);
        CHECK(newDict->get("Mood"_sl)->asString() == "happy"_sl);
        CHECK(newDict->get("Size"_sl)->asString() == "XXXL"_sl);
        CHECK(newDict->get("Vehicle"_sl)->asString() == "top"_sl);
        CHECK(newDict->get("Asleep"_sl) == nullptr);
        CHECK(newDict->get("Q"_sl) == nullptr);

        {
            Dict::iterator i(newDict);
            CHECKiter(i, "Friend", "catbus");
            CHECKiter(i, "Mood", "happy");
            CHECKiter(i, "Name", "totoro");
            CHECKiter(i, "Size", "XXXL");
            CHECKiter(i, "Vehicle", "top");
            CHECK(!i);
        }
        //CHECK(newDict->rawCount() == 4);
        CHECK(newDict->count() == 5);

        std::cerr << "\nContents:      " << newDict->toJSON().asString() << "\n";
    }


    TEST_CASE("Larger mutable dict", "[Mutable]") {
        mmap_slice data(kTestFilesDir "1person.fleece");
        auto person = Value::fromTrustedData(data)->asDict();

        std::cerr << "Original data: " << data << "\n";
        std::cerr << "Contents:      " << person->toJSON().asString() << "\n";
        Value::dump(data, std::cerr);

        Retained<MutableDict> mp = MutableDict::newDict(person);
        mp->set("age"_sl, 31);
        MutableArray *friends = mp->getMutableArray("friends"_sl);
        REQUIRE(friends);
//        auto frend = friends->getMutableDict(1);
//        REQUIRE(frend);
//        frend->set("name"_sl, "Reddy Kill-a-Watt"_sl);

        Encoder enc;
        enc.setBase(data);
        enc.reuseBaseStrings();
        enc.writeValue(mp);
        alloc_slice data2 = enc.extractOutput();

        alloc_slice combined(data);
        combined.append(data2);
        const Dict* newDict = Value::fromData(combined)->asDict();
        std::cerr << "\n\nContents:      " << newDict->toJSON().asString() << "\n";
        std::cerr << "Delta:         " << data2 << "\n\n";
        Value::dump(combined, std::cerr);
    }

}
