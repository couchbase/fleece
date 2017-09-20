//
//  MutableTests.cc
//  Fleece
//
//  Created by Jens Alfke on 9/19/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "FleeceTests.hh"
#include "MutableArray.hh"

namespace fleece {

    TEST_CASE("MutableArray attributes", "[Mutable]") {
        MutableArray ma;
        const Value *v = &ma;

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
        CHECK(v->asMutableArray() == &ma);
    }


    TEST_CASE("MutableArray set values", "[Mutable]") {
        MutableArray ma;
        const Array *a = &ma;

        REQUIRE(a->count() == 0);
        //REQUIRE(a->empty());  //FIX!
        REQUIRE(a->get(0) == nullptr);

        {
            Array::iterator i(a);
            CHECK(!i);
        }

        ma.resize(9);
        REQUIRE(ma.count() == 9);
        REQUIRE(a->count() == 9);
        REQUIRE(!a->empty());

        for (int i = 0; i < 9; i++)
            REQUIRE(a->get(i)->type() == kNull);

        ma.setNull(0);
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
            CHECK(a->get(i)->type() == kExpectedTypes[i]);
        CHECK(a->get(1)->asBool() == false);
        CHECK(a->get(2)->asBool() == true);
        CHECK(a->get(3)->asInt() == 0);
        CHECK(a->get(4)->asInt() == -123);
        CHECK(a->get(5)->asInt() == 2017);
        CHECK(a->get(6)->asInt() == 123456789);
        CHECK(a->get(7)->asInt() == -123456789);
        CHECK(a->get(8)->asString() == "Hot dog"_sl);

        {
            Array::iterator i(a);
            for (int n = 0; n < 9; ++n) {
                std::cerr << "Item " << n << ": " << (void*)i.value() << "\n";
                CHECK(i);
                CHECK(i.value() != nullptr);
                CHECK(i.value()->type() == kExpectedTypes[n]);
                ++i;
            }
            CHECK(!i);
        }

        CHECK(a->toJSON() == "[null,false,true,0,-123,2017,123456789,-123456789,\"Hot dog\"]"_sl);

        ma.remove(3, 5);
        CHECK(a->count() == 4);
        CHECK(a->get(2)->type() == kBoolean);
        CHECK(a->get(2)->asBool() == true);
        CHECK(a->get(3)->type() == kString);

        ma.insert(1, 2);
        CHECK(a->count() == 6);
        REQUIRE(a->get(1)->type() == kNull);
        REQUIRE(a->get(2)->type() == kNull);
        CHECK(a->get(3)->type() == kBoolean);
        CHECK(a->get(3)->asBool() == false);
    }


    TEST_CASE("MutableArray pointers", "[Mutable]") {
        MutableArray ma;
        ma.resize(2);
        ma.set(0, 123);
        ma.set(1, 456);

        MutableArray mb;
        mb.append(&ma);

        CHECK(mb.get(0) == &ma);
        CHECK(mb.makeArrayMutable(0) == &ma);

        Encoder enc;
        enc.beginArray();
        enc << "totoro";
        enc << "catbus";
        enc.endArray();
        alloc_slice data = enc.extractOutput();
        const Array* fleeceArray = Value::fromData(data)->asArray();

        CHECK(fleeceArray->asMutableArray() == nullptr);

        mb.append(fleeceArray);
        CHECK(mb.get(1) == fleeceArray);
        auto mc = mb.makeArrayMutable(1);
        CHECK(mc != nullptr);
        CHECK(mc == mb.get(1));
        CHECK(mb.get(1)->type() == kArray);

        CHECK(mc->count() == 2);
        CHECK(((const Array*)mc)->count() == 2);
        CHECK(mc->get(0)->asString() == "totoro"_sl);
        CHECK(mc->get(1)->asString() == "catbus"_sl);
    }
}

