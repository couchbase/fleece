//
// EncoderTests.cpp
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

#include "FleeceTests.hh"
#include "Pointer.hh"
#include "JSONConverter.hh"
#include "KeyTree.hh"
#include "Path.hh"
#include "Internal.hh"
#include "jsonsl.h"
#include "mn_wordlist.h"
#include "NumConversion.hh"
#include <iostream>
#include <float.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

namespace fleece { namespace impl {
    using namespace fleece::impl::internal;

class EncoderTests {
public:
    EncoderTests()
    :enc()
    { }

    ~EncoderTests() {
        enc.reset();
    }

    Encoder enc;
    alloc_slice result;

    void endEncoding() {
        enc.end();
        result = enc.finish();
        enc.reset();
    }

    template <bool WIDE>
    uint32_t pointerOffset(const Value *v) const noexcept {
        return v->_asPointer()->offset<WIDE>();
    }

    void checkOutput(const char *expected) {
        endEncoding();
        std::string hex;
        for (size_t i = 0; i < result.size; i++) {
            char str[4];
            sprintf(str, "%02X", result[i]);
            hex.append(str);
            if (i % 2 && i != result.size-1)
                hex.append(" ");
        }
        REQUIRE(hex == std::string(expected));
    }

    void checkReadBool(bool b) {
        auto v = Value::fromData(result);
        REQUIRE(v != nullptr);
        REQUIRE(v->type() == kBoolean);
        REQUIRE(v->asBool() == b);
        REQUIRE(v->asInt() == (int64_t)b);
    }

    void checkRead(int64_t i) {
        auto v = Value::fromData(result);
        if (!v || v->type() != kNumber || !v->isInteger() || v->isUnsigned() || v->asInt() != i || v->asDouble() != (double)i) {
        REQUIRE(v != nullptr);
        REQUIRE(v->type() == kNumber);
        REQUIRE(v->isInteger());
        REQUIRE(!v->isUnsigned());
        REQUIRE(v->asInt() == i);
        REQUIRE(v->asDouble() == (double)i);
    }
    }

    void checkReadU(uint64_t i) {
        auto v = Value::fromData(result);
        if (!v || v->type() != kNumber || !v->isInteger() || v->asUnsigned() != i || v->asDouble() != (double)i) {
            REQUIRE(v != nullptr);
        REQUIRE(v->type() == kNumber);
        REQUIRE(v->isInteger());
            REQUIRE(v->asUnsigned() == i);
            REQUIRE(v->asDouble() == (double)i);
        }
        if (i >= (UINT64_MAX >> 1))
            REQUIRE(v->isUnsigned());
    }

    void checkReadFloat(float f) {
        auto v = Value::fromData(result);
        REQUIRE(v != nullptr);
        REQUIRE(v->type() == kNumber);
        REQUIRE(!v->isDouble());
        REQUIRE(v->asInt() == (int64_t)f);
        REQUIRE(v->asFloat() == f);
        REQUIRE(v->asDouble() == (double)f);
    }

    void checkReadDouble(double f) {
        auto v = Value::fromData(result);
        REQUIRE(v != nullptr);
        REQUIRE(v->type() == kNumber);
        REQUIRE(v->asInt() == (int64_t)f);
        REQUIRE(v->asDouble() == f);
        REQUIRE(v->asFloat() == (float)f);
    }

    void checkReadString(const char *str) {
        auto v = Value::fromData(result);
        REQUIRE(v != nullptr);
        REQUIRE(v->type() == kString);
        REQUIRE(v->asString() == slice(str, strlen(str)));
    }

    const Array* checkArray(uint32_t count) {
        auto v = Value::fromData(result);
        REQUIRE(v != nullptr);
        REQUIRE(v->type() == kArray);
        auto a = v->asArray();
        REQUIRE(a != nullptr);
        REQUIRE(a->count() == count);
        return a;
    }

    const Dict* checkDict(uint32_t count) {
        auto v = Value::fromData(result);
        REQUIRE(v != nullptr);
        REQUIRE(v->type() == kDict);
        auto d = v->asDict();
        REQUIRE(d != nullptr);
        REQUIRE(d->count() == count);
        return d;
    }

    void testArrayOfLength(unsigned length) {
        enc.beginArray();
        for (unsigned i = 0; i < length; ++i)
            enc.writeUInt(i);
        enc.endArray();
        endEncoding();

        // Check the contents:
        auto a = checkArray(length);
        for (unsigned i = 0; i < length; ++i) {
            auto v = a->get(i);
            if (!v || v->type() != kNumber || v->asUnsigned() != (uint64_t)i) {
                REQUIRE(v);
                REQUIRE(v->type() == kNumber);
                REQUIRE(v->asUnsigned() == (uint64_t)i);
            }
        }
    }

    void checkJSONStr(std::string json,
                      slice expectedStr,
                      int expectedErr = JSONSL_ERROR_SUCCESS)
    {
        json = std::string("[\"") + json + std::string("\"]");
        JSONConverter j(enc);
        j.encodeJSON(slice(json));
        REQUIRE(j.jsonError() == expectedErr);
        if (j.jsonError()) {
            enc.reset();
            return;
        }
        endEncoding();
        REQUIRE(expectedStr); // expected success
        auto a = checkArray(1);
        auto output = a->get(0)->asString();
        REQUIRE(output == slice(expectedStr));
    }

    void checkJSONStr(std::string json,
                      const char *expectedStr,
                      int expectedErr = JSONSL_ERROR_SUCCESS)
    {
        checkJSONStr(json, (slice)expectedStr, expectedErr);
    }

    void lookupNameWithKey(const Dict* person, Dict::key &nameKey, std::string expectedName) {
        REQUIRE(person);
        const Value *name = person->get(nameKey);
        REQUIRE(name);
        std::string nameStr = (std::string)name->asString();
        REQUIRE(nameStr == expectedName);

        // Second lookup (using cache)
        name = person->get(nameKey);
        REQUIRE(name);
        nameStr = (std::string)name->asString();
        REQUIRE(nameStr == expectedName);
    }

    void writeKey(int key) {
        enc.writeKey(key);
    }

};

#pragma mark - TESTS

    TEST_CASE_METHOD(EncoderTests, "Empty", "[Encoder]") {
        REQUIRE(enc.isEmpty());
        enc.beginArray();
        REQUIRE(!enc.isEmpty());
        enc.endArray();

        Encoder enc2;
        REQUIRE(enc2.isEmpty());
        enc2 << 17;
        REQUIRE(!enc2.isEmpty());

        enc2.reset();
        REQUIRE(enc2.isEmpty());
    }

    TEST_CASE_METHOD(EncoderTests, "Pointer", "[Encoder]") {
        const uint8_t data[2] = {0x80, 0x02};
        auto v = (const Value*)data;
        REQUIRE(pointerOffset<false>(v) == 4u);
    }

    TEST_CASE_METHOD(EncoderTests, "Special", "[Encoder]") {
        enc.writeNull();        checkOutput("3000");
        enc.writeBool(false);   checkOutput("3400");    checkReadBool(false);
        enc.writeBool(true);    checkOutput("3800");    checkReadBool(true);
    }

    TEST_CASE_METHOD(EncoderTests, "Ints", "[Encoder][Numeric]") {
        enc.writeInt( 0);       checkOutput("0000");    checkRead(0);
        enc.writeInt( 128);     checkOutput("0080");    checkRead(128);
        enc.writeInt( 1234);    checkOutput("04D2");    checkRead(1234);
        enc.writeInt(-1234);    checkOutput("0B2E");    checkRead(-1234);
        enc.writeInt( 2047);    checkOutput("07FF");    checkRead(2047);
        enc.writeInt(-2048);    checkOutput("0800");    checkRead(-2048);
        enc.writeInt( 2048);    checkOutput("1100 0800 8002");    checkRead(2048);
        enc.writeInt(-2049);    checkOutput("11FF F700 8002");    checkRead(-2049);

#if !FL_EMBEDDED     // this takes too long on a puny microcontroller
        for (int i = -66666; i <= 66666; ++i) {
            enc.writeInt(i);
            endEncoding();
            checkRead(i);
        }
        for (unsigned i = 0; i <= 66666; ++i) {
            enc.writeUInt(i);
            endEncoding();
            checkReadU(i);
        }
#endif

        enc.writeInt(12345678); checkOutput("134E 61BC 0000 8003"); checkRead(12345678);
        enc.writeInt(-12345678);checkOutput("13B2 9E43 FF00 8003"); checkRead(-12345678);
        enc.writeInt(0x223344); checkOutput("1244 3322 8002");      checkRead(0x223344);
        enc.writeInt(0xBBCCDD); checkOutput("13DD CCBB 0000 8003"); checkRead(0xBBCCDD);
        enc.writeInt(0x11223344556677);    checkOutput("1677 6655 4433 2211 8004");
        checkRead(0x11223344556677);
        enc.writeInt(0x1122334455667788);  checkOutput("1788 7766 5544 3322 1100 8005");
        checkRead(0x1122334455667788);
        enc.writeInt(-0x1122334455667788); checkOutput("1778 8899 AABB CCDD EE00 8005");
        checkRead(-0x1122334455667788);
        enc.writeUInt(0xCCBBAA9988776655); checkOutput("1F55 6677 8899 AABB CC00 8005");
        checkReadU(0xCCBBAA9988776655);
        enc.writeUInt(UINT64_MAX);         checkOutput("1FFF FFFF FFFF FFFF FF00 8005");
        checkReadU(UINT64_MAX);

        for (int bits = 0; bits < 64; ++bits) {
            int64_t i = 1LL << bits;
            enc.writeInt(i);      endEncoding();  checkRead(i);
            if (bits < 63) {
                enc.writeInt(-i);     endEncoding();  checkRead(-i);
                enc.writeInt(i - 1);  endEncoding();  checkRead(i - 1);
                enc.writeInt(1 - i);  endEncoding();  checkRead(1 - i);
            }
        }
        for (int bits = 0; bits < 64; ++bits) {
            uint64_t i = 1LLU << bits;
            enc.writeUInt(i);     endEncoding();  checkReadU(i);
            enc.writeUInt(i - 1); endEncoding();  checkReadU(i - 1);
        }
    }

    TEST_CASE_METHOD(EncoderTests, "Floats", "[Encoder][Numeric]") {
        enc.writeFloat( 0.5);   checkOutput("2000 0000 003F 8003");           checkReadFloat( 0.5);
        enc.writeFloat(-0.5);   checkOutput("2000 0000 00BF 8003");           checkReadFloat(-0.5);
        enc.writeFloat((float)M_PI); checkOutput("2000 DB0F 4940 8003");      checkReadFloat((float)M_PI);
        enc.writeDouble(M_PI);  checkOutput("2800 182D 4454 FB21 0940 8005"); checkReadDouble(M_PI);

        // Floats that get encoded as integers:
        enc.writeFloat(0.0);       checkOutput("0000");              checkReadFloat(0.0);
        enc.writeFloat(-2048.0);   checkOutput("0800");              checkReadFloat(-2048.0);
        enc.writeFloat(0x223344);  checkOutput("1244 3322 8002");    checkReadFloat(0x223344);

        // Doubles that get encoded as integers:
        enc.writeDouble(0.0);       checkOutput("0000");              checkReadDouble(0.0);
        enc.writeDouble(-2048.0);   checkOutput("0800");              checkReadDouble(-2048.0);
        enc.writeDouble(0x223344);  checkOutput("1244 3322 8002");    checkReadDouble(0x223344);

        // Doubles that get encoded as float:
        enc.writeDouble( 0.5);   checkOutput("2000 0000 003F 8003");           checkReadDouble( 0.5);
        enc.writeDouble(-0.5);   checkOutput("2000 0000 00BF 8003");           checkReadDouble(-0.5);
        enc.writeDouble((float)M_PI); checkOutput("2000 DB0F 4940 8003");      checkReadDouble((float)M_PI);
}

    TEST_CASE_METHOD(EncoderTests, "Strings", "[Encoder]") {
        enc.writeString("");    checkOutput("4000");            checkReadString("");
        enc.writeString("a");   checkOutput("4161");            checkReadString("a");
        enc.writeString("ab");  checkOutput("4261 6200 8002");  checkReadString("ab");
        enc.writeString("abcdefghijklmn");
        checkOutput("4E61 6263 6465 6667 6869 6A6B 6C6D 6E00 8008");
        checkReadString("abcdefghijklmn");
        enc.writeString("abcdefghijklmno");
        checkOutput("4F0F 6162 6364 6566 6768 696A 6B6C 6D6E 6F00 8009");
        checkReadString("abcdefghijklmno");
        enc.writeString("abcdefghijklmnop");
        checkOutput("4F10 6162 6364 6566 6768 696A 6B6C 6D6E 6F70 8009");
        checkReadString("abcdefghijklmnop");

        enc.writeString("müßchop"); checkOutput("496D C3BC C39F 6368 6F70 8005");
        checkReadString("müßchop");

        // Check a long string (long enough that length has multi-byte varint encoding):
        char cstr[667];
        memset(cstr, '@', 666);
        cstr[666] = 0;
        std::string str = cstr;
        enc.writeString(str);
        endEncoding();
        checkReadString(cstr);
    }

    TEST_CASE_METHOD(EncoderTests, "Arrays", "[Encoder]") {
        {
            enc.beginArray();
            enc.endArray();
            checkOutput("6000");
            checkArray(0);
        }
        {
            enc.beginArray(1);
            enc.writeNull();
            enc.endArray();
            checkOutput("6001 3000 8002");
            auto a = checkArray(1);
            auto v = a->get(0);
            REQUIRE(v);
            REQUIRE(v->type() == kNull);
        }
        {
            enc.beginArray(2);
            enc.writeString("a");
            enc.writeString("hello");
            enc.endArray();
            checkOutput("4568 656C 6C6F 6002 4161 8005 8003");
            // Check the contents:
            auto a = checkArray(2);
            auto v = a->get(0);
            REQUIRE(v);
            REQUIRE(v->type() == kString);
            REQUIRE(v->asString() == "a"_sl);
            v = a->get(1);
            REQUIRE(v);
            REQUIRE(v->type() == kString);
            REQUIRE(v->asString() == "hello"_sl);

            // Now use an iterator:
            Array::iterator iter(a);
            REQUIRE(iter);
            REQUIRE(iter->type() == kString);
            REQUIRE(iter->asString() == slice("a"));
            ++iter;
            REQUIRE(iter);
            REQUIRE(iter->type() == kString);
            REQUIRE(iter->asString() == slice("hello"));
            ++iter;
            REQUIRE(!iter);

            REQUIRE(a->toJSON() == alloc_slice("[\"a\",\"hello\"]"));
        }
#if 0
        {
            // Strings that can be inlined in a wide array:
            enc.beginArray(2);
            enc.writeString("ab");
            enc.writeString("cde");
            enc.endArray();
            checkOutput("6802 4261 6200 4363 6465 8005");
        }
#endif
    }

    TEST_CASE_METHOD(EncoderTests, "LongArrays", "[Encoder]") {
        testArrayOfLength(0x7FE);
        testArrayOfLength(0x7FF);
        testArrayOfLength(0x800);
        testArrayOfLength(0x801);
#if !FL_EMBEDDED
        testArrayOfLength(0xFFFF);
#endif
    }

    TEST_CASE_METHOD(EncoderTests, "Dictionaries", "[Encoder]") {
        {
            enc.beginDictionary();
            enc.endDictionary();
            checkOutput("7000");
            checkDict(0);
        }
        {
            enc.beginDictionary();
            enc.writeKey("f");
            enc.writeInt(42);
            enc.endDictionary();
            checkOutput("7001 4166 002A 8003");
            auto d = checkDict(1);
            auto v = d->get(slice("f"));
            REQUIRE(v);
            REQUIRE(v->asInt() == 42ll);
            REQUIRE(d->get(slice("barrr")) == (const Value*)nullptr);
            REQUIRE(d->toJSON() == alloc_slice("{\"f\":42}"));
            REQUIRE(d->toJSON<5>() == alloc_slice("{f:42}"));
        }
        {
            enc.beginDictionary();
            enc.writeKey("o-o");
            enc.writeInt(42);
            enc.endDictionary();
            checkOutput("436F 2D6F 7001 8003 002A 8003");
            auto d = checkDict(1);
            auto v = d->get(slice("o-o"));
            REQUIRE(v);
            REQUIRE(v->asInt() == 42);
            REQUIRE(d->get(slice("barrr")) == (const Value*)nullptr);
            REQUIRE(d->toJSON() == alloc_slice("{\"o-o\":42}"));
            REQUIRE(d->toJSON<5>() == alloc_slice("{\"o-o\":42}"));
        }
    }

#ifndef NDEBUG
    TEST_CASE_METHOD(EncoderTests, "DictionaryNumericKeys", "[Encoder]") {
        gDisableNecessarySharedKeysCheck = true;
        {
            enc.beginDictionary();
            writeKey(0);
            enc.writeInt(23);
            writeKey(1);
            enc.writeInt(42);
            writeKey(2047);
            enc.writeInt(-1);
            enc.endDictionary();
            checkOutput("7003 0000 0017 0001 002A 07FF 0FFF 8007");
            auto d = checkDict(3);
            auto v = d->get(0);
            REQUIRE(v);
            REQUIRE(v->asInt() == 23ll);
            v = d->get(1);
            REQUIRE(v);
            REQUIRE(v->asInt() == 42ll);
            v = d->get(2047);
            REQUIRE(v);
            REQUIRE(v->asInt() == -1ll);
            REQUIRE(d->get(slice("barrr")) == (const Value*)nullptr);
            REQUIRE(d->toJSON() == alloc_slice("{0:23,1:42,2047:-1}"));
        }
        gDisableNecessarySharedKeysCheck = false;
    }
#endif

    TEST_CASE_METHOD(EncoderTests, "Deep Nesting", "[Encoder]") {
        for (int depth = 0; depth < 100; ++depth) {
            enc.beginArray();
            enc.writeInt(depth);
        }
        for (int depth = 0; depth < 100; ++depth) {
            char str[20];
            sprintf(str, "Hi there %d", depth);
            enc.writeString(str);
            enc.endArray();
        }
        endEncoding();
    }

    TEST_CASE_METHOD(EncoderTests, "SharedStrings", "[Encoder]") {
        enc.beginArray(4);
        enc.writeString("a");
        enc.writeString("hello");
        enc.writeString("a");
        enc.writeString("hello");
        enc.endArray();
        checkOutput("4568 656C 6C6F 6004 4161 8005 4161 8007 8005");
        auto a = checkArray(4);
        REQUIRE(a->toJSON() == alloc_slice("[\"a\",\"hello\",\"a\",\"hello\"]"));
    }

#if !FL_EMBEDDED
    TEST_CASE("Widening Edge Case", "[Encoder]") {
        // Tests an edge case in the Encoder's logic for widening an array/dict when a pointer
        // reaches back 64KB. See couchbase/couchbase-lite-core#493
        static constexpr size_t kMinStringLen = 60000, kMaxStringLen = 70000;
        char *string = new char[kMaxStringLen];
        memset(string, 'x', kMaxStringLen);
        for (size_t stringLen = kMinStringLen; stringLen <= kMaxStringLen; ++stringLen) {
            Encoder enc;
            enc.beginArray();
            enc.writeString("hi");
            enc.writeString("there");
            enc.writeString(slice{string, stringLen});
            enc.endArray();
            auto data = enc.finish();
        }
        delete [] string;
    }
#endif

#pragma mark - JSON:

    TEST_CASE_METHOD(EncoderTests, "JSONStrings", "[Encoder]") {
        checkJSONStr("", "");
        checkJSONStr("x", "x");
        checkJSONStr("\\\"", "\"");
        checkJSONStr("\"", nullptr, JSONConverter::kErrTruncatedJSON); // unterminated string
        checkJSONStr("\\", nullptr, JSONConverter::kErrTruncatedJSON);
        checkJSONStr("hi \\\"there\\\"", "hi \"there\"");
        checkJSONStr("hi\\nthere", "hi\nthere");
        checkJSONStr("H\\u0061ppy", "Happy");
        checkJSONStr("H\\u0061", "Ha");

        // Unicode escapes:
        checkJSONStr("Price 50\\u00A2", "Price 50¢");
        checkJSONStr("Price \\u20ac250", "Price €250");
        checkJSONStr("Price \\uffff?", "Price \uffff?");
        checkJSONStr("Price \\u20ac", "Price €");
        checkJSONStr("!\\u0000!", "!\0!"_sl);
        checkJSONStr("Price \\u20a", nullptr, JSONSL_ERROR_UESCAPE_TOOSHORT);
        checkJSONStr("Price \\u20", nullptr, JSONSL_ERROR_UESCAPE_TOOSHORT);
        checkJSONStr("Price \\u2", nullptr, JSONSL_ERROR_UESCAPE_TOOSHORT);
        checkJSONStr("Price \\u", nullptr, JSONSL_ERROR_UESCAPE_TOOSHORT);
        checkJSONStr("\\uzoop!", nullptr, JSONSL_ERROR_PERCENT_BADHEX);

        // UTF-16 surrogate pair decoding:
        checkJSONStr("lmao\\uD83D\\uDE1C!", "lmao😜!");
        checkJSONStr("lmao\\uD83D", nullptr, JSONSL_ERROR_INVALID_CODEPOINT);
        checkJSONStr("lmao\\uD83D\\n", nullptr, JSONSL_ERROR_INVALID_CODEPOINT);
        checkJSONStr("lmao\\uD83D\\u", nullptr, JSONSL_ERROR_UESCAPE_TOOSHORT);
        checkJSONStr("lmao\\uD83D\\u333", nullptr, JSONSL_ERROR_UESCAPE_TOOSHORT);
        checkJSONStr("lmao\\uD83D\\u3333", nullptr, JSONSL_ERROR_INVALID_CODEPOINT);
        checkJSONStr("lmao\\uDE1C\\uD83D!", nullptr, JSONSL_ERROR_INVALID_CODEPOINT);
    }

    TEST_CASE_METHOD(EncoderTests, "JSON", "[Encoder]") {
        slice json("{\"\":\"hello\\nt\\\\here\","
                            "\"\\\"ironic\\\"\":[null,false,true,-100,0,100,123.456,6.02e+23,5e-06],"
                            "\"foo\":123}");
        JSONConverter j(enc);
        REQUIRE(j.encodeJSON(json));
        endEncoding();
        auto d = checkDict(3);
        auto output = d->toJSON();
        REQUIRE((slice)output == json);
    }

    TEST_CASE_METHOD(EncoderTests, "JSON parse numbers", "[Encoder]") {
        slice json = "[9223372036854775807, -9223372036854775808, 18446744073709551615, "
                       "18446744073709551616, 602214076000000000000000, "
                       "-9999999999999999999]"_sl;
        alloc_slice data = JSONConverter::convertJSON(json);
        const Array *root = Value::fromTrustedData(data)->asArray();
        CHECK(root->get(0)->isInteger());
        CHECK(root->get(0)->asInt() == INT64_MAX);
        CHECK(root->get(1)->isInteger());
        CHECK(root->get(1)->asInt() == INT64_MIN);

        CHECK(root->get(2)->isInteger());
        CHECK(root->get(2)->asUnsigned() == UINT64_MAX);

        CHECK(!root->get(3)->isInteger());
        CHECK(root->get(3)->asDouble() == 18446744073709551616.0);
        CHECK(!root->get(4)->isInteger());
        CHECK(root->get(4)->asDouble() == 6.02214076e23);
        CHECK(!root->get(5)->isInteger());
        CHECK(root->get(5)->asDouble() == -9999999999999999999.0);
    }

    TEST_CASE_METHOD(EncoderTests, "JSONBinary", "[Encoder]") {
        enc.beginArray();
        enc.writeData(slice("not-really-binary"));
        enc.endArray();
        endEncoding();
        auto json = Value::fromData(result)->toJSON();
        REQUIRE(json == alloc_slice("[\"bm90LXJlYWxseS1iaW5hcnk=\"]"));

        Writer w;
        w.writeDecodedBase64(slice("bm90LXJlYWxseS1iaW5hcnk="));
        REQUIRE(w.finish() == alloc_slice("not-really-binary"));
    }

    TEST_CASE_METHOD(EncoderTests, "Dump", "[Encoder]") {
        std::string json = json5("{'foo':123,"
                                 "'\"ironic\"':[null,false,true,-100,0,100,123.456,6.02e+23],"
                                 "'':'hello\\nt\\\\here'}");
        JSONConverter j(enc);
        j.encodeJSON(slice(json));
        endEncoding();
        std::string dumped = Value::dump(result);
        //std::cerr << dumped;
        REQUIRE(dumped == std::string(
            "0000: 43 66 6f 6f : \"foo\"\n"
            "0004: 48 22 69 72…: \"\\\"ironic\\\"\"\n"
            "000e: 28 00 77 be…: 123.456\n"
            "0018: 28 00 61 d3…: 6.02e+23\n"
            "0022: 60 08       : Array[8]:\n"
            "0024: 30 00       :   null\n"
            "0026: 34 00       :   false\n"
            "0028: 38 00       :   true\n"
            "002a: 0f 9c       :   -100\n"
            "002c: 00 00       :   0\n"
            "002e: 00 64       :   100\n"
            "0030: 80 11       :   &123.456 (@000e)\n"
            "0032: 80 0d       :   &6.02e+23 (@0018)\n"
            "0034: 4c 68 65 6c…: \"hello\\nt\\\\here\"\n"
            "0042: 70 03       : Dict[3]:\n"
            "0044: 40 00       :   \"\"\n"
            "0046: 80 09       :     &\"hello\\nt\\\\here\" (@0034)\n"
            "0048: 80 22       :   &\"\\\"ironic\\\"\" (@0004)\n"
            "004a: 80 14       :     &Array[8] (@0022)\n"
            "004c: 80 26       :   &\"foo\" (@0000)\n"
            "004e: 00 7b       :     123\n"
            "0050: 80 07       : &Dict[3] (@0042)\n"));
    }

    TEST_CASE_METHOD(EncoderTests, "ConvertPeople", "[Encoder]") {
        auto input = readTestFile(kBigJSONTestFileName);

        enc.uniqueStrings(true);

        JSONConverter jr(enc);
        if (!jr.encodeJSON(input))
            FAIL("JSON parse error at " << jr.errorPos());

#if 0
        // Dump the string table and some statistics:
        auto &strings = enc.strings();
        strings.dump();
#endif

        enc.end();
        result = enc.finish();

#if FL_HAVE_TEST_FILES
        REQUIRE(result.buf);
        writeToFile(result, kTestFilesDir "1000people.fleece");
#endif

        fprintf(stderr, "\nJSON size: %zu bytes; Fleece size: %zu bytes (%.2f%%)\n",
                input.size, result.size, (result.size*100.0/input.size));
#ifndef NDEBUG
        fprintf(stderr, "Narrow: %u, Wide: %u (total %u)\n", enc._numNarrow, enc._numWide, enc._numNarrow+enc._numWide);
        fprintf(stderr, "Narrow count: %u, Wide count: %u (total %u)\n", enc._narrowCount, enc._wideCount, enc._narrowCount+enc._wideCount);
        fprintf(stderr, "Used %u pointers to shared strings\n", enc._numSavedStrings);
#endif
    }

#if FL_HAVE_TEST_FILES
    TEST_CASE_METHOD(EncoderTests, "Encode To File", "[Encoder]") {
    	auto doc = readTestFile("1000people.fleece");
        auto root = Value::fromTrustedData(doc)->asArray();

        {
            FILE *out = fopen(kTempDir"fleecetemp.fleece", "wb");
            Encoder fenc(out);
            fenc.writeValue(root);
            fenc.end();
            fclose(out);
        }

        alloc_slice newDoc = readFile(kTempDir"fleecetemp.fleece");
        REQUIRE(newDoc);
        auto newRoot = Value::fromData(newDoc)->asArray();
        REQUIRE(newRoot);
        CHECK(newRoot->count() == root->count());
    }
#endif

#if FL_HAVE_TEST_FILES
    TEST_CASE_METHOD(EncoderTests, "FindPersonByIndexSorted", "[Encoder]") {
        auto doc = readTestFile("1000people.fleece");
        auto root = Value::fromTrustedData(doc)->asArray();
        auto person = root->get(123)->asDict();
        const Value *name = person->get(slice("name"));
        REQUIRE(name);
        std::string nameStr = (std::string)name->asString();
        REQUIRE(nameStr == std::string("Concepcion Burns"));
    }
#endif

    TEST_CASE_METHOD(EncoderTests, "FindPersonByIndexKeyed", "[Encoder]") {
        {
            Dict::key nameKey(slice("name"));

            // First build a small non-wide Dict:
            enc.beginArray();
                enc.beginDictionary();
                    enc.writeKey("f");
                    enc.writeInt(42);
                    enc.writeKey("name");
                    enc.writeString("Concepcion Burns");
                    enc.writeKey("x");
                    enc.writeBool(false);
                enc.endDictionary();
                enc.beginDictionary();
                    enc.writeKey("name");
                    enc.writeString("Carmen Miranda");
                    enc.writeKey("x");
                    enc.writeBool(false);
                enc.endDictionary();
                enc.beginDictionary();
                    enc.writeKey("nxme");
                    enc.writeString("Carmen Miranda");
                    enc.writeKey("x");
                    enc.writeBool(false);
                enc.endDictionary();
            enc.endArray();
            endEncoding();
            auto smol = Value::fromData(result)->asArray();
            lookupNameWithKey(smol->get(0)->asDict(), nameKey, "Concepcion Burns");
            lookupNameWithKey(smol->get(1)->asDict(), nameKey, "Carmen Miranda");
            REQUIRE(smol->get(2)->asDict()->get(nameKey) == nullptr);
        }
#if FL_HAVE_TEST_FILES
        {
            // Now try a wide Dict:
            Dict::key nameKey(slice("name"));

            auto doc = readTestFile("1000people.fleece");
            auto root = Value::fromTrustedData(doc)->asArray();
            auto person = root->get(123)->asDict();
            lookupNameWithKey(person, nameKey, "Concepcion Burns");

            person = root->get(3)->asDict();
            lookupNameWithKey(person, nameKey, "Isabella Compton");
        }
#endif
    }

    TEST_CASE_METHOD(EncoderTests, "Paths", "[Encoder]") {
        auto input = readTestFile(kBigJSONTestFileName);
        JSONConverter jr(enc);
        jr.encodeJSON(input);
        enc.end();
        alloc_slice fleeceData = enc.finish();
        const Value *root = Value::fromData(fleeceData);
        CHECK(root->asArray()->count() == kBigJSONTestCount);

        Path p1{"$[32].name"};
        const Value *name = p1.eval(root);
        REQUIRE(name);
        REQUIRE(name->type() == kString);
        REQUIRE(name->asString() == slice("Mendez Tran"));

        Path p2{"[-1].name"};
        name = p2.eval(root);
        REQUIRE(name);
        REQUIRE(name->type() == kString);
#if FL_HAVE_TEST_FILES
        REQUIRE(name->asString() == slice("Marva Morse"));
#else  // embedded test uses only 50 people, not 1000, so [-1] resolves differently
        REQUIRE(name->asString() == slice("Tara Wall"));
#endif
    }

    TEST_CASE_METHOD(EncoderTests, "Resuse Encoder", "[Encoder]") {
        enc.beginDictionary();
        enc.writeKey("foo");
        enc.writeInt(17);
        enc.endDictionary();
        auto data1 = enc.finish();

        enc.beginDictionary();
        enc.writeKey("bar");
        enc.writeInt(23);
        enc.endDictionary();
        auto data2 = enc.finish();

        enc.beginDictionary();
        enc.writeKey("baz");
        enc.writeInt(42);
        enc.endDictionary();
        auto data3 = enc.finish();
    }

    TEST_CASE_METHOD(EncoderTests, "Multi-Item", "[Encoder]") {
        enc.suppressTrailer();
        size_t pos[10];
        unsigned n = 0;

        enc.beginDictionary();
        enc.writeKey("foo");
        enc.writeInt(17);
        enc.endDictionary();
        pos[n++] = enc.finishItem();

        enc.beginDictionary();
        enc.writeKey("bar");
        enc.writeInt(123456789);
        enc.endDictionary();
        pos[n++] = enc.finishItem();

        enc.beginArray();
        enc.writeBool(false);
        enc.writeBool(true);
        enc.endArray();
        pos[n++] = enc.finishItem();

        enc.writeString("LOL BUTTS"_sl);
        pos[n++] = enc.finishItem();

        enc.writeString("X"_sl);
        pos[n++] = enc.finishItem();

        enc.writeInt(17);
        pos[n++] = enc.finishItem();

        endEncoding();
        pos[n] = result.size;
        for (unsigned i = 0; i < n; i++)
            CHECK(pos[i] < pos[i+1]);
        CHECK(result.size == pos[n-1] + 2);

        auto dict = (const Dict*)&result[pos[0]];
        REQUIRE(dict->type() == kDict);
        CHECK(dict->count() == 1);
        REQUIRE(dict->get("foo"_sl));
        CHECK(dict->get("foo"_sl)->asInt() == 17);

        dict = (const Dict*)&result[pos[1]];
        REQUIRE(dict->type() == kDict);
        CHECK(dict->count() == 1);
        REQUIRE(dict->get("bar"_sl));
        CHECK(dict->get("bar"_sl)->asInt() == 123456789);

        auto array = (const Array*)&result[pos[2]];
        REQUIRE(array->type() == kArray);
        REQUIRE(array->count() == 2);
        CHECK(array->get(0)->type() == kBoolean);
        CHECK(array->get(1)->type() == kBoolean);

        auto str = (const Value*)&result[pos[3]];
        REQUIRE(str->type() == kString);
        CHECK(str->asString() == "LOL BUTTS"_sl);

        str = (const Value*)&result[pos[4]];
        REQUIRE(str->type() == kString);
        CHECK(str->asString() == "X"_sl);

        auto num = (const Value*)&result[pos[5]];
        REQUIRE(num->type() == kNumber);
        CHECK(num->asInt() == 17);
    }

#pragma mark - KEY TREE:

    TEST_CASE_METHOD(EncoderTests, "KeyTree", "[Encoder]") {
        bool verbose = false;

        char eeeeeeee[1024] = "";
        memset(&eeeeeeee[0], 'e', sizeof(eeeeeeee)-1);

#if 1
        const size_t n = sizeof(mn_words)/sizeof(char*);
        const char* rawStrings[n];
        memcpy(rawStrings, mn_words, sizeof(mn_words));
        rawStrings[0] = eeeeeeee;
#else
        const char* rawStrings[] = {"alphabetically first", "bravo", "charlie", "delta", eeeeeeee,
            "foxtrot", "ganon in pig form as slain by link at the end of ocarina of time", "hi",
            "i", "jodhpur",
            "kale is not one of my favorite vegetables, too bitter, though I respect its high vitamin/mineral content",
            "lemon", "maxwell edison, majoring in medicine, calls her on the phone", "naomi",
            "obey", "purple", "quorum", "roger", "snake", "tango", "umpqua", "vector", "whiskey",
            "xerxes", "yellow", "zebra"};
        size_t n = sizeof(rawStrings)/sizeof(char*);
#endif
        std::vector<slice> strings(n);
        size_t totalLen = 0;
        for (size_t i = 0; i < n; ++i) {
            strings[i] = slice(rawStrings[i]);
            totalLen += strings[i].size;
        }

        KeyTree keys = KeyTree::fromStrings(strings);
        slice output = keys.encodedData();
        if (verbose) {
            std::cerr << "\n" << sliceToHexDump(output, 32);
        }
        std::cerr << "Size = " << output.size << "; that's " << output.size-totalLen << " bytes overhead for " << n << " strings, i.e." << (output.size-totalLen)/(double)n << " bytes/string.\n";

        std::vector<bool> ids(n+1);
        for (size_t i = 0; i < n; ++i) {
            INFO( "Checking '" << rawStrings[i] << "' ... ");
            unsigned id = keys[strings[i]];
            REQUIRE(id);
            REQUIRE(!ids[id]);
            ids[id] = true;

            slice lookup = keys[id];
            INFO("    id = " << id << ", lookup = " << lookup );
            REQUIRE(lookup.buf);
            REQUIRE(lookup == strings[i]);
        }

        REQUIRE(keys[slice("")] == 0);
        REQUIRE(keys[slice("foo")] == 0);
        REQUIRE(keys[slice("~")] == 0);
        REQUIRE(keys[slice("whiske")] == 0);
        REQUIRE(keys[slice("whiskex")] == 0);
        REQUIRE(keys[slice("whiskez")] == 0);

        REQUIRE(keys[0].buf == nullptr);
        REQUIRE(keys[(unsigned)n+1].buf == nullptr);
        REQUIRE(keys[(unsigned)n+2].buf == nullptr);
        REQUIRE(keys[(unsigned)n+28].buf == nullptr);
        REQUIRE(keys[(unsigned)9999].buf == nullptr);
    }

    TEST_CASE("Locale-free encoding") {
        // Note this will fail if Linux is missing the French locale,
        // so make sure it is installed on the machine doing testing
        
        char doubleBuf[32];
        char floatBuf[32];
        double testDouble = M_PI;
        float testFloat = 2.71828f;
        sprintf(doubleBuf, "%.16g", testDouble);
        sprintf(floatBuf, "%.7g", testFloat);
        CHECK((strcmp(doubleBuf, "3.141592653589793")) == 0);
        CHECK((strcmp(floatBuf, "2.71828")) == 0);

        WriteFloat(testDouble, doubleBuf, 32);
        WriteFloat(testFloat, floatBuf, 32);
        CHECK((strcmp(doubleBuf, "3.141592653589793")) == 0);
        CHECK((strcmp(floatBuf, "2.71828")) == 0);

        double recovered = ParseDouble(doubleBuf);
        float recovered_f = (float)ParseDouble(floatBuf);
        CHECK(DoubleEquals(recovered, M_PI));
        CHECK(FloatEquals(recovered_f, 2.71828f));

#ifdef _MSC_VER
        setlocale(LC_ALL, "fr-FR");
#else
        setlocale(LC_ALL, "fr_FR");
#endif

        sprintf(doubleBuf, "%.16g", testDouble);
        sprintf(floatBuf, "%.7g", testFloat);
        CHECK((strcmp(doubleBuf, "3,141592653589793")) == 0);
        CHECK((strcmp(floatBuf, "2,71828")) == 0);

        WriteFloat(testDouble, doubleBuf, 32);
        WriteFloat(testFloat, floatBuf, 32);
        CHECK((strcmp(doubleBuf, "3.141592653589793")) == 0);
        CHECK((strcmp(floatBuf, "2.71828")) == 0);

        recovered = strtod(doubleBuf, nullptr);
        recovered_f = (float)strtod(floatBuf, nullptr);
        CHECK(!DoubleEquals(recovered, M_PI)); // Locale dependent, incorrect result
        CHECK(!FloatEquals(recovered_f, 2.71828f));

        recovered = ParseDouble(doubleBuf);
        recovered_f = (float)ParseDouble(floatBuf);
        CHECK(DoubleEquals(recovered, M_PI)); // Locale independent, correct result
        CHECK(FloatEquals(recovered_f, 2.71828f));

        setlocale(LC_ALL, "C");
    }


    TEST_CASE("ParseInteger unsigned") {
        constexpr const char* kTestCases[] = {
            "0", "1", "9", "  99 ", "+12345", "  +12345",
            "18446744073709551615", // UINT64_MAX
        };

        uint64_t result;
        for (const char *str : kTestCases) {
            INFO("Checking \"" << str << "\"");
            bool parsed = ParseInteger(str, result);
            CHECK(parsed);
            if (parsed)
                CHECK(result == strtoull(str, nullptr, 10));
        }

        constexpr const char* kFailCases[] = {
            "", " ", "+", " +", " + ", "x", " x", "1234x", "1234 x", "123.456", "-17", " + 1234"
            "18446744073709551616" // UINT64_MAX + 1
        };

        for (const char *str : kFailCases) {
            INFO("Checking \"" << str << "\"");
            CHECK(!ParseInteger(str, result));
        }
    }


    TEST_CASE("ParseInteger signed") {
        constexpr const char* kTestCases[] = {
            "0", "1", "9", "  99 ", "+17",
            "+0", "-0", "-1", "+12", " -12345",
             "9223372036854775807",  // INT64_MAX
            "-9223372036854775808",  // INT64_MIN
        };
        int64_t result;
        for (const char *str : kTestCases) {
            INFO("Checking \"" << str << "\"");
            bool parsed = ParseInteger(str, result);
            CHECK(parsed);
            if (parsed)
                CHECK(result == strtoll(str, nullptr, 10));
        }

        constexpr const char* kFailCases[] = {
            "", " ", "x", " x", "1234x", "1234 x", "123.456", "18446744073709551616",
            "-", " - ", "-+", "- 1",
             "9223372036854775808",  // INT64_MAX + 1
            "-9223372036854775809"   // INT64_MIN - 1
        };
        for (const char *str : kFailCases) {
            INFO("Checking \"" << str << "\"");
            CHECK(!ParseInteger(str, result));
        }
    }

} }
