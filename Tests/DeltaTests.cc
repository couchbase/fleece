//
// DeltaTests.cc
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
#include "Delta.hh"

using namespace fleece;


static std::string toJSONString(const Value *v) {
    return v ? v->toJSONString() : "undefined";
}


static void checkDelta(const char *json1, const char *json2, const char *deltaExpected) {
    // Parse json1 and json2:
    const Value *v1 = nullptr, *v2 = nullptr;
    alloc_slice f1, f2;
    if (json1) {
        auto j = ConvertJSON5(std::string(json1));
        j = std::string("[") + j + "]";
        f1 = JSONConverter::convertJSON(slice(j));
        v1 = Value::fromData(f1);
        REQUIRE(v1);
        v1 = v1->asArray()->get(0);
    }
    if (json2) {
        auto j = ConvertJSON5(std::string(json2));
        j = std::string("[") + j + "]";
        f2 = JSONConverter::convertJSON(slice(j));
        v2 = Value::fromData(f2);
        REQUIRE(v2);
        v2 = v2->asArray()->get(0);
    }

    // Compute the delta and check it:
    alloc_slice jsonDelta = CreateDelta(v1, v2, true);
    CHECK(jsonDelta == slice(deltaExpected));

    if (jsonDelta.size > 0) {
        // Now apply the delta to the old value to get the new one:
        alloc_slice f2_reconstituted = ApplyDelta(v1, jsonDelta, true);
        auto v2_reconstituted = Value::fromData(f2_reconstituted);
        INFO("v2 reconstituted:  " << toJSONString(v2_reconstituted) << "   original:  " << toJSONString(v2));
        CHECK(v2_reconstituted->isEqual(v2));
    }
}


TEST_CASE("Delta scalars", "[delta]") {
    checkDelta("null", "null", nullptr);
    checkDelta("''", "''", nullptr);
    checkDelta("5", "5", nullptr);
    checkDelta("5", "6", "[0,6]");
    checkDelta("false", "0", "[0,0]");
    checkDelta("'hi'", "'Hi'", "[0,\"Hi\"]");
}


TEST_CASE("Delta strings", "[delta]") {
    checkDelta("'hi'", "7", "[0,7]");
    checkDelta("7", "'hi'", "[0,\"hi\"]");
    checkDelta("'hi'", "'there'", "[0,\"there\"]");
    checkDelta("'to wound the autumnal city. So howled out for the world to give him a name.  The in-dark answered with the wind.'",
               "'To wound the eternal city. So he howled out for the world to give him its name. The in-dark answered with wind.'",
               "[\"1-1+T|12=5-4+eter|13=3+he |37=1-3+its|6=1-27=4-5=\",0,2]");
    checkDelta("'to wound the autumnal city. The in-dark answered with the wind.'",
               "'to wound the autumnal city. So howled out for the world to give him a name. The in-dark answered with the wind.'",
               "[\"27=48+ So howled out for the world to give him a name.|36=\",0,2]");
}


TEST_CASE("Delta simple dicts", "[delta]") {
    checkDelta("{}", "{}", nullptr);
    checkDelta("{foo: 1}", "{foo: 1}", nullptr);
    checkDelta("{foo: 1, bar: 2, baz: 3}", "{foo: 1, bar: 2, baz: 3}", nullptr);

    checkDelta("{}", "{bar: 2}", "{bar:[2]}");
    checkDelta("{foo: 1}", "{}", "{foo:[]}");
    checkDelta("{foo: 1}", "{bar: 2}", "{bar:[2],foo:[]}");
    checkDelta("{foo: 1}", "{foo: 2}", "{foo:[0,2]}");
    checkDelta("{foo: 1}", "{foo: 1, bar: 2}", "{bar:[2]}");
    checkDelta("{foo: 1, bar: 2, baz: 3}", "{foo: 1, bar: 17, baz: 3}", "{bar:[0,17]}");
}


TEST_CASE("Delta arrays", "[delta]") {
    // There are no optimizations for array deltas yet; it just treats it as a value change
    checkDelta("[1, 2]", "[1, 3]", "[0,[1,3]]");
}


TEST_CASE("Delta nested dicts", "[delta]") {
    checkDelta("{foo: {bar: [1], baz:{goo:[3]},wow:0}}", "{foo: {bar: [1], baz:{goo:[3]},wow:0}}", nullptr);
    checkDelta("{foo: {bar: [1]}, goo: 2}", "{foo: {bar: [1]}, goo: 3}", "{goo:[0,3]}");
    checkDelta("{foo: {bar: [1]}, goo: 2}", "{foo: {bar: [2]}, goo: 2}", "{foo:{bar:[0,[2]]}}");
    checkDelta("{foo: {bar: [1]}, goo: [2]}", "{foo: {bar: [2]}, goo: [3]}", "{foo:{bar:[0,[2]]},goo:[0,[3]]}");
}


static void checkDelta(const Value *left, const Value *right, const Value *expectedDelta) {
    alloc_slice jsonDelta = CreateDelta(left, right);
    alloc_slice fleeceDelta = JSONConverter::convertJSON(jsonDelta);
    const Value *delta = Value::fromData(fleeceDelta);
    INFO("Delta of " << toJSONString(left) << "  -->  " << toJSONString(right) << "  ==  " << toJSONString(expectedDelta) << "  ...  got  " << toJSONString(delta));
    if (expectedDelta)
        CHECK(expectedDelta->isEqual(delta));
    else
        CHECK(delta == 0);
}


TEST_CASE("JSONDiffPatch test suite", "[delta]") {
    Encoder enc;
    auto input = readTestFile("DeltaTests.json5");
    JSONConverter jr(enc);
    jr.encodeJSON(ConvertJSON5(std::string(input)));
    enc.end();
    alloc_slice encoded = enc.extractOutput();
    const Dict *testSuites = Value::fromData(encoded)->asDict();
    REQUIRE(testSuites);

    gCompatibleDeltas = true;

    for (Dict::iterator i_suite(testSuites); i_suite; ++i_suite) {
        std::cerr << "        * " << std::string(i_suite.keyString()) << "\n";
        if (i_suite.keyString() == "arrays"_sl) {
            std::cerr << "            SKIPPED\n";
            continue; // TODO: Enable array tests once array diffs are implemented
        }
        const Array *tests = i_suite.value()->asArray();
        int i = 1;
        for (Array::iterator i_test(tests); i_test; ++i_test, ++i) {
            const Dict *test = i_test.value()->asDict();
            if (!test)
                continue;
            const Value *name = test->get("name"_sl);
            if (name )
                std::cerr << "          - " << std::string(name->asString()) << "\n";
            else
                std::cerr << "          - " << i << "\n";

            // OK, run a test:
            auto left = test->get("left"_sl), right = test->get("right"_sl);
            checkDelta(left,  right, test->get("delta"_sl));
            checkDelta(right, left,  test->get("reverse"_sl));
        }
    }

    gCompatibleDeltas = false;
}
