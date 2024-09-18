//
// DeltaTests.cc
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "FleeceTests.hh"
#include "FleeceImpl.hh"
#include "JSONDelta.hh"
#include <iostream>

namespace fleece { namespace impl {
    extern bool gCompatibleDeltas;
} }

using namespace fleece;
using namespace fleece::impl;
using namespace fleece_test;


static bool isValidUTF8(fleece::slice sl) noexcept;

static std::string toJSONString(const Value *v) {
    return v ? v->toJSONString() : "undefined";
}


static void checkDelta(const char *json1, const char *json2, const char *deltaExpected) {
    auto sk = retained(new SharedKeys());

    if (!deltaExpected)
        deltaExpected = "{}";
    // Parse json1 and json2:
    Retained<Doc> doc1, doc2;
    const Value *v1 = nullptr, *v2 = nullptr;
    if (json1) {
        auto j = ConvertJSON5(std::string(json1));
        j = std::string("[") + j + "]";
        doc1 = Doc::fromJSON(slice(j), sk);
        v1 = doc1->root()->asArray()->get(0);
    }
    if (json2) {
        auto j = ConvertJSON5(std::string(json2));
        j = std::string("[") + j + "]";
        doc2 = Doc::fromJSON(slice(j), sk);
        v2 = doc2->root()->asArray()->get(0);
    }

    // Compute the delta and check it:
    alloc_slice jsonDelta = JSONDelta::create(v1, v2, true);
    std::cerr << "Delta: " << std::string(jsonDelta) << "\n";
    CHECK(isValidUTF8(jsonDelta));
    CHECK(jsonDelta == slice(deltaExpected));

    if (jsonDelta.size > 0) {
        // Now apply the delta to the old value to get the new one:
        alloc_slice f2_reconstituted = JSONDelta::apply(v1, jsonDelta, true);
        auto v2_reconstituted = Value::fromData(f2_reconstituted);
        INFO("value2 reconstituted:  " << toJSONString(v2_reconstituted) << " ;  should be:  " << toJSONString(v2) << " ;  delta: " << jsonDelta);
        CHECK(v2_reconstituted->isEqual(v2));
    }
}

#ifdef __cpp_char8_t
static void checkDelta(const char8_t *json1, const char8_t *json2, const char8_t *deltaExpected) {
    checkDelta((const char*)json1, (const char*)json2, (const char*)deltaExpected);
}
#endif

TEST_CASE("Delta scalars", "[delta]") {
    checkDelta("null", "null", nullptr);
    checkDelta("''", "''", nullptr);
    checkDelta("5", "5", nullptr);
    checkDelta("5", "6", "[6]");        // wrapped in array because "6" is not valid JSON document
    checkDelta("false", "[]", "[[]]");
    checkDelta("'hi'", "'Hi'", "[\"Hi\"]"); // ditto
}


TEST_CASE("Delta strings", "[delta]") {
    JSONDelta::gMinStringDiffLength = 36;
    JSONDelta::gTextDiffTimeout = -1; // No timeout since it has platform-specific effects

    // Empty string
    checkDelta("'hi'", "''", "[\"\"]");
    // No change
    checkDelta("'there'", "'there'", nullptr);
    // Single word
    checkDelta("'hi'", "'there'", "[\"there\"]");
    // Change one word
    checkDelta("'Hello World.'",
               "'Goodbye World.'",
               "[\"Goodbye World.\"]");
    // Too short for delta
    checkDelta("'The fog comes in on little cat feet'",
               "'The dog comes in on little cat feet'",
               "[\"The dog comes in on little cat feet\"]");
    // Modify string
    checkDelta("'to wound the autumnal city. So howled out for the world to give him a name.  The in-dark answered with the wind.'",
               "'To wound the eternal city. So he howled out for the world to give him its name. The in-dark answered with wind.'",
               "[\"1-1+T|12=5-4+eter|13=3+he |37=1-3+its|6=1-27=4-5=\",0,2]");
    // Insert in middle
    checkDelta("'to wound the autumnal city. The in-dark answered with the wind.'",
               "'to wound the autumnal city. So howled out for the world to give him a name. The in-dark answered with the wind.'",
               "[\"27=48+ So howled out for the world to give him a name.|36=\",0,2]");
    // Inefficient delta
    checkDelta("'Lorem ipsum dolor sit amet, assueverit sadipscing usu ea, mei efficiantur intellegebat in, iudico ullamcorper ei ius. Ius quaeque eripuit instructior ea, et ipsum doctus quo, pri decore ornatus et. Te wisi omittantur interpretaris quo, in audire prompta nominati vim. Dicat epicuri delectus sit eu.'",
               "'Ex quo prima efficiantur, an pro modus pertinax. Magna tractatos qualisque vim id. Eum at omnis inani, labore possim nec id. Exerci audire eam eu, summo liberavisse mel ei. Homero ponderum ea his, cum id impedit fuisset.'",
               "[\"Ex quo prima efficiantur, an pro modus pertinax. Magna tractatos qualisque vim id. Eum at omnis inani, labore possim nec id. Exerci audire eam eu, summo liberavisse mel ei. Homero ponderum ea his, cum id impedit fuisset.\"]");
    // Delta control chars in string
    checkDelta("'ABC+DEF-HIJ=KLM|NOP *******************************'",
               "'AbC-def+HIJKLM|NOP= *******************************'",
               "[\"1=7-7+bC-def+|3=1-7=1+=|32=\",0,2]");

    JSONDelta::gMinStringDiffLength = 60;
}

TEST_CASE("Delta strings UTF-8", "[delta]") {
    // See issue #40 -- JSONDelta must take care not to put a string patch boundary in the middle
    // of a UTF-8 multibyte character sequence, or the output will contain invalid UTF-8.
    JSONDelta::gMinStringDiffLength = 1;

    // Multi-byte UTF-8 chars, with patches occurring in midst of UTF-8 sequences:
    checkDelta(u8"'モバイルデータベースは将来のものです。 ある日、私たちのデータが端に集まります。'",
               u8"'モバイルデータベースがここにあります。 あなたのデータはすべて端にあります。'",
               u8"[\"30=49-37+がここにあります。 あなた|12=3-12+はすべて|6=3-3-3+あ|12=\",0,2]");

    // Here the C7/C6 bytes can't be included in the preceding XXX/YYY diff:
    checkDelta("'<aaaaaaaaXXX\xC7\x88zzzzzzzz>'",
               "'<aaaaaaaaYYY\xC6\x88zzzzzzzz>'",
               "[\"9=5-5+YYY\xC6\x88|9=\",0,2]");

    checkDelta(u8"'யாமறிந்த மொழிகளிலே தமிழ்மொழி போல் இனிதாவது எங்கும் காணோம், பாமரராய் விலங்குகளாய், உலகனைத்தும் இகழ்ச்சிசொலப் பான்மை கெட்டு, நாமமது தமிழரெனக் கொண்டு இங்கு வாழ்ந்திடுதல் நன்றோ? சொல்லீர்! தேமதுரத் இகழ்ச்சிசொலப் உலகமெலாம் பரவும்வகை செய்தல் வேண்டும்.'",
               u8"'யாமறிந்த மொழிகளிலே தமிழ்மொழி போல் இனிதாவது எங்கும் காணோம், பாமரராய் விலங்குகளாய், உலகனைத்தும் இகழ்ச்சிசொலப் பான்மை கெட்டு, நாமமது தமிழரெனக் கொண்டு இங்கு வாழ்ந்திடுதல் நன்றோ? கொண்டு! தேமதுரத் தமிழோசை உலகமெலாம் பரவும்வகை செய்தல் வேண்டும்.'",
               u8"[\"476=3-3+க|3=3-3+ண|3=12-6+டு|27=21-6+தம|3=15-12+ழோசை|104=\",0,2]");

    JSONDelta::gMinStringDiffLength = 60;
}


TEST_CASE("Delta simple dicts", "[delta]") {
    checkDelta("{}", "{}", nullptr);
    checkDelta("{foo: 1}", "{foo: 1}", nullptr);
    checkDelta("{foo: 1, bar: 2, baz: 3}", "{baz: 3, foo: 1, bar: 2}", nullptr);

    checkDelta("{}", "{bar: 2}", "{bar:2}");
    checkDelta("{foo: 1}", "{}", "{foo:[]}");
    checkDelta("{foo: 1}", "{bar: 2}", "{bar:2,foo:[]}");
    checkDelta("{foo: 1}", "{foo: 2}", "{foo:2}");
    checkDelta("{foo: 1}", "{foo: 1, bar: 2}", "{bar:2}");
    checkDelta("{foo: 1, bar: 2, baz: 3}", "{foo: 1, bar: 17, baz: 3}", "{bar:17}");

    checkDelta("{foo: 1}", "[2]", "[[2]]");
    checkDelta("[2]", "{foo: 1}", "[{foo:1}]");
    checkDelta("{top: {foo: 1}}", "{top: [2]}", "{top:[[2]]}");
    checkDelta("{top: [2]}", "{top: {foo: 1}}", "{top:[{foo:1}]}");
}


TEST_CASE("Delta nested dicts", "[delta]") {
    checkDelta("{}", "{bar: {baz: 9}}", "{bar:[{baz:9}]}");
    checkDelta("{foo: {bar: [1], baz:{goo:[3]},wow:0}}", "{foo: {bar: [1], baz:{goo:[3]},wow:0}}", nullptr);
    checkDelta("{foo: {bar: [1]}, goo: 2}", "{foo: {bar: [1]}, goo: 3}", "{goo:3}");
    checkDelta("{foo: {bar: [1]}, goo: 2}", "{foo: {bar: [2]}, goo: 2}", "{foo:{bar:{\"0\":2}}}");
    checkDelta("{quuz: true, foo:{bar:{buzz:\"qux\"}}}", "{quuz: true, foo:{bar:{buzz:\"quux\"}}}", "{foo:{bar:{buzz:\"quux\"}}}");
    checkDelta("{foo: 1, bar: 2, baz: [\"A\", \"B\", \"C\"]}", "{foo: 1, bar: 2, baz: {A: 1, B: 2, C: 3}}", "{baz:[{A:1,B:2,C:3}]}");
    checkDelta("{foo: {bar: [1]}, goo: [2]}", "{foo: {bar: [2]}, goo: [3]}", "{foo:{bar:{\"0\":2}},goo:{\"0\":3}}");
    checkDelta("{\"glossary\":{\"title\":\"example glossary\",\"GlossDiv\":{\"title\":\"S\",\"GlossList\":{\"GlossEntry\":[{\"ID\":\"SGML\",\"SortAs\":\"SGML\",\"GlossTerm\":\"Standard Generalized Markup Language\",\"Acronym\":\"SGML\",\"Abbrev\":\"ISO 8879:1986\",\"GlossDef\":{\"para\":\"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.\",\"GlossSeeAlso\":[\"GML\",\"XML\"]}},{\"ID\":\"SGML\",\"SortAs\":\"SGML\",\"GlossTerm\":\"Standard Generalized Markup Language\",\"Acronym\":\"SGML\",\"Abbrev\":\"ISO 8879:1986\",\"GlossDef\":{\"para\":\"A meta-markup language, used to create markup languages such as DocBook.\",\"GlossSeeAlso\":[\"GML\",\"XML\"]}}],\"GlossSee\":\"markup\"}}}}",
               "{\"glossary\":{\"title\":\"example glossary\",\"GlossDiv\":{\"title\":\"S\",\"GlossList\":{\"GlossEntry\":[{\"ID\":\"SGML\",\"SortAs\":\"SGML\",\"GlossTerm\":\"Standard Generalized Markup Language\",\"Acronym\":\"SGML\",\"Abbrev\":\"ISO 8879:1986\",\"GlossDef\":{\"para\":\"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit sint cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.\",\"GlossSeeAlso\":[\"GML\",\"XML\"]}},{\"ID\":\"SGML\",\"SortAs\":\"SGML\",\"GlossTerm\":\"Standard Generalized Markup Language\",\"Acronym\":\"SGML\",\"Abbrev\":\"ISO 8879:1986\",\"GlossDef\":{\"para\":\"A meta-markup language, used to create markup languages such as DocBook.\",\"GlossSeeAlso\":[\"GML\",\"XML\"]}}],\"GlossSee\":\"markup\"}}}}",
               "{glossary:{GlossDiv:{GlossList:{GlossEntry:{\"0\":{GlossDef:{para:[\"290=4-4+sint|151=\",0,2]}}}}}}}");
}


TEST_CASE("Delta simple arrays", "[delta]") {
    checkDelta("[]", "[]", nullptr);
    checkDelta("[1, 2, 3]", "[1, 2, 3]", nullptr);

    checkDelta("[]", "[1, 2, 3]", "[[1,2,3]]");
    checkDelta("[1, 2, 3]", "[]", "[[]]");
    checkDelta("[1, 2, 3, 5, 6, 7]", "[1, 2, 3, 4, 5]", "{\"3\":4,\"4\":5,\"5-\":[]}"); // non-optimal - could be {"3-":[4,5]}
    checkDelta("[1, 2, 3]", "[1, 2, 3, 4, 5]", "{\"3-\":[4,5]}");
    checkDelta("[1, 2, 3, 4, 5]", "[1, 2, 3]", "{\"3-\":[]}");
    checkDelta("[1, 2, 3]", "[1, 9, 3]", "{\"1\":9}");
    checkDelta("[1, 2, 3]", "[4, 5, 6]", "{\"0\":4,\"1\":5,\"2\":6}");
    checkDelta("['Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.']", "['Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, sed nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.']", "{\"0\":[\"149=4-3+sed|78=\",0,2]}");
}


TEST_CASE("Delta nested arrays", "[delta]") {
    checkDelta("[[[]]]", "[[[]]]", nullptr);
    checkDelta("[1,[2,[3]]]", "[1,[2,[3]]]", nullptr);
    checkDelta("[1, [21, 22], 3]", "[1, [21, 222], 3]", "{\"1\":{\"1\":222}}");
    checkDelta("[1, [21, 22], 3]", "[1, [21, 22, 23], 3]", "{\"1\":{\"2-\":[23]}}");
    checkDelta("[1, {'hi':'there'}, 3]", "[1, {'hi':'ho'}, 3]", "{\"1\":{hi:\"ho\"}}");
}


static void checkDelta(const Value *left, const Value *right, const Value *expectedDelta) {
    if (!expectedDelta)
        expectedDelta = Dict::kEmpty;
    alloc_slice jsonDelta = JSONDelta::create(left, right);
    alloc_slice fleeceDelta = JSONConverter::convertJSON(jsonDelta);
    const Value *delta = Value::fromData(fleeceDelta);
    INFO("Delta of " << toJSONString(left) << "  -->  " << toJSONString(right) << "  ==  " << toJSONString(expectedDelta) << "  ...  got  " << toJSONString(delta));
    if (expectedDelta)
        CHECK(expectedDelta->isEqual(delta));
    else
        CHECK(delta == nullptr);
}


TEST_CASE("JSONDiffPatch test suite", "[delta]") {
    Encoder enc;
    auto input = readTestFile("DeltaTests.json5");
    JSONConverter jr(enc);
    jr.encodeJSON(ConvertJSON5(std::string(input)));
    enc.end();
    alloc_slice encoded = enc.finish();
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



// Based on utf8_check.c by Markus Kuhn, 2005
// https://www.cl.cam.ac.uk/~mgk25/ucs/utf8_check.c
static bool isValidUTF8(fleece::slice sl) noexcept
{
    auto s = (const uint8_t*)sl.buf;
    for (auto e = s + sl.size; s != e; ) {
        while (!(*s & 0x80)) {
            if (++s == e) {
                return true;
            }
        }

        if ((s[0] & 0x60) == 0x40) {
            if (s + 1 >= e || (s[1] & 0xc0) != 0x80 || (s[0] & 0xfe) == 0xc0) {
                return false;
            }
            s += 2;
        } else if ((s[0] & 0xf0) == 0xe0) {
            if (s + 2 >= e || (s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
                (s[0] == 0xe0 && (s[1] & 0xe0) == 0x80) || (s[0] == 0xed && (s[1] & 0xe0) == 0xa0)) {
                return false;
            }
            s += 3;
        } else if ((s[0] & 0xf8) == 0xf0) {
            if (s + 3 >= e || (s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 || (s[3] & 0xc0) != 0x80 ||
                (s[0] == 0xf0 && (s[1] & 0xf0) == 0x80) || (s[0] == 0xf4 && s[1] > 0x8f) || s[0] > 0xf4) {
                return false;
            }
            s += 4;
        } else {
            return false;
        }
    }
    return true;
}
