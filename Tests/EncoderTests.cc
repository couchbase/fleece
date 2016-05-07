//
//  EncoderTests.cpp
//  Fleece
//
//  Created by Jens Alfke on 11/14/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//

#include "FleeceTests.hh"
#include "JSONConverter.hh"

namespace fleece {

class EncoderTests : public CppUnit::TestFixture {
public:
    EncoderTests()
    :enc(writer)
    { }

    ~EncoderTests() {
        enc.reset();
    }

    Writer writer;
    Encoder enc;
    alloc_slice result;

    void endEncoding() {
        enc.end();
        result = writer.extractOutput();
        enc.reset();
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
        AssertEqual(hex, std::string(expected));
    }

    void checkReadBool(bool b) {
        auto v = value::fromData(result);
        Assert(v != NULL);
        Assert(v->type() == kBoolean);
        AssertEqual(v->asBool(), b);
        AssertEqual(v->asInt(), (int64_t)b);
    }

    void checkRead(int64_t i) {
        auto v = value::fromData(result);
        Assert(v != NULL);
        Assert(v->type() == kNumber);
        Assert(v->isInteger());
        Assert(!v->isUnsigned());
        AssertEqual(v->asInt(), i);
        AssertEqual(v->asDouble(), (double)i);
    }

    void checkReadU(uint64_t i) {
        auto v = value::fromData(result);
        Assert(v->type() == kNumber);
        Assert(v->isInteger());
        Assert(v->isUnsigned());
        AssertEqual(v->asUnsigned(), i);
        AssertEqual(v->asDouble(), (double)i);
    }

    void checkReadFloat(float f) {
        auto v = value::fromData(result);
        Assert(v != NULL);
        Assert(v->type() == kNumber);
        Assert(!v->isDouble());
        AssertEqual(v->asInt(), (int64_t)round(f));
        AssertEqual(v->asFloat(), f);
        AssertEqual(v->asDouble(), (double)f);
    }

    void checkReadDouble(double f) {
        auto v = value::fromData(result);
        Assert(v != NULL);
        Assert(v->type() == kNumber);
        AssertEqual(v->asInt(), (int64_t)round(f));
        AssertEqual(v->asDouble(), f);
        AssertEqual(v->asFloat(), (float)f);
    }

    void checkReadString(const char *str) {
        auto v = value::fromData(result);
        Assert(v != NULL);
        Assert(v->type() == kString);
        AssertEqual(v->asString(), slice(str, strlen(str)));
    }

    const array* checkArray(uint32_t count) {
        auto v = value::fromData(result);
        Assert(v != NULL);
        Assert(v->type() == kArray);
        auto a = v->asArray();
        Assert(a != NULL);
        AssertEqual(a->count(), count);
        return a;
    }

    const dict* checkDict(uint32_t count) {
        auto v = value::fromData(result);
        Assert(v != NULL);
        Assert(v->type() == kDict);
        auto d = v->asDict();
        Assert(d != NULL);
        AssertEqual(d->count(), count);
        return d;
    }

#pragma mark - TESTS

    void testPointer() {
        uint8_t data[2] = {0x80, 0x02};
        auto v = (const value*)data;
        AssertEqual(v->pointerValue<false>(), 4u);
    }

    void testSpecial() {
        enc.writeNull();        checkOutput("3000");
        enc.writeBool(false);   checkOutput("3400");    checkReadBool(false);
        enc.writeBool(true);    checkOutput("3800");    checkReadBool(true);
    }

    void testInts() {
        enc.writeInt( 0);       checkOutput("0000");    checkRead(0);
        enc.writeInt( 1234);    checkOutput("04D2");    checkRead(1234);
        enc.writeInt(-1234);    checkOutput("0B2E");    checkRead(-1234);
        enc.writeInt( 2047);    checkOutput("07FF");    checkRead(2047);
        enc.writeInt(-2048);    checkOutput("0800");    checkRead(-2048);

        enc.writeInt( 2048);    checkOutput("1100 0800 8002");    checkRead(2048);
        enc.writeInt(-2049);    checkOutput("11FF F700 8002");    checkRead(-2049);
        enc.writeInt(0x223344); checkOutput("1244 3322 8002");    checkRead(0x223344);
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
    }

    void testFloats() {
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

    void testStrings() {
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

    void testArrays() {
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
            Assert(v);
            AssertEqual(v->type(), kNull);
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
            Assert(v);
            AssertEqual(v->type(), kString);
            AssertEqual(v->asString(), slice("a"));
            v = a->get(1);
            Assert(v);
            AssertEqual(v->type(), kString);
            AssertEqual(v->asString(), slice("hello"));

            // Now use an iterator:
            array::iterator iter(a);
            Assert(iter);
            AssertEqual(iter->type(), kString);
            AssertEqual(iter->asString(), slice("a"));
            ++iter;
            Assert(iter);
            AssertEqual(iter->type(), kString);
            AssertEqual(iter->asString(), slice("hello"));
            ++iter;
            Assert(!iter);

            AssertEqual(a->toJSON(), alloc_slice("[\"a\",\"hello\"]"));
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

    void testDictionaries() {
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
            Assert(v);
            AssertEqual(v->asInt(), 42ll);
            AssertEqual(d->get(slice("barrr")), (const value*)NULL);
            AssertEqual(d->toJSON(), alloc_slice("{\"f\":42}"));
        }
        {
            enc.beginDictionary();
            enc.writeKey("foo");
            enc.writeInt(42);
            enc.endDictionary();
            checkOutput("4366 6F6F 7001 8003 002A 8003");
            auto d = checkDict(1);
            auto v = d->get(slice("foo"));
            Assert(v);
            AssertEqual(v->asInt(), 42ll);
            AssertEqual(d->get(slice("barrr")), (const value*)NULL);
            AssertEqual(d->toJSON(), alloc_slice("{\"foo\":42}"));
        }
    }

    void testSharedStrings() {
        enc.beginArray(4);
        enc.writeString("a");
        enc.writeString("hello");
        enc.writeString("a");
        enc.writeString("hello");
        enc.endArray();
        checkOutput("4568 656C 6C6F 6004 4161 8005 4161 8007 8005");
        auto a = checkArray(4);
        AssertEqual(a->toJSON(), alloc_slice("[\"a\",\"hello\",\"a\",\"hello\"]"));
    }

#pragma mark - JSON:

    void checkJSONStr(std::string json,
                      const char *expectedStr,
                      int expectedErr = JSONSL_ERROR_SUCCESS)
    {
        json = std::string("[\"") + json + std::string("\"]");
        JSONConverter j(enc);
        j.convertJSON(slice(json));
        AssertEqual(j.error(), expectedErr);
        if (j.error()) {
            enc.reset();
            return;
        }
        endEncoding();
        Assert(expectedStr); // expected success
        auto a = checkArray(1);
        auto output = a->get(0)->asString();
        AssertEqual(output, slice(expectedStr));
    }

    void testJSONStrings() {
        checkJSONStr("", "");
        checkJSONStr("x", "x");
        checkJSONStr("\\\"", "\"");
        checkJSONStr("\"", NULL, JSONConverter::kErrTruncatedJSON); // unterminated string
        checkJSONStr("\\", NULL, JSONConverter::kErrTruncatedJSON);
        checkJSONStr("hi \\\"there\\\"", "hi \"there\"");
        checkJSONStr("hi\\nthere", "hi\nthere");
        checkJSONStr("H\\u0061ppy", "Happy");
        checkJSONStr("H\\u0061", "Ha");

        // Unicode escapes:
        checkJSONStr("Price 50\\u00A2", "Price 50¢");
        checkJSONStr("Price \\u20ac250", "Price €250");
        checkJSONStr("Price \\uffff?", "Price \uffff?");
        checkJSONStr("Price \\u20ac", "Price €");
        checkJSONStr("Price \\u20a", NULL, JSONSL_ERROR_UESCAPE_TOOSHORT);
        checkJSONStr("Price \\u20", NULL, JSONSL_ERROR_UESCAPE_TOOSHORT);
        checkJSONStr("Price \\u2", NULL, JSONSL_ERROR_UESCAPE_TOOSHORT);
        checkJSONStr("Price \\u", NULL, JSONSL_ERROR_UESCAPE_TOOSHORT);
        checkJSONStr("\\uzoop!", NULL, JSONSL_ERROR_PERCENT_BADHEX);
        checkJSONStr("!\\u0000!", NULL, JSONSL_ERROR_INVALID_CODEPOINT);

        // UTF-16 surrogate pair decoding:
        checkJSONStr("lmao\\uD83D\\uDE1C!", "lmao😜!");
        checkJSONStr("lmao\\uD83D", NULL, JSONSL_ERROR_INVALID_CODEPOINT);
        checkJSONStr("lmao\\uD83D\\n", NULL, JSONSL_ERROR_INVALID_CODEPOINT);
        checkJSONStr("lmao\\uD83D\\u", NULL, JSONSL_ERROR_UESCAPE_TOOSHORT);
        checkJSONStr("lmao\\uD83D\\u333", NULL, JSONSL_ERROR_UESCAPE_TOOSHORT);
        checkJSONStr("lmao\\uD83D\\u3333", NULL, JSONSL_ERROR_INVALID_CODEPOINT);
        checkJSONStr("lmao\\uDE1C\\uD83D!", NULL, JSONSL_ERROR_INVALID_CODEPOINT);
    }

    void testJSON() {
        slice json("{\"\":\"hello\\nt\\\\here\","
                            "\"\\\"ironic\\\"\":[null,false,true,-100,0,100,123.456,6.02e+23],"
                            "\"foo\":123}");
        JSONConverter j(enc);
        Assert(j.convertJSON(json));
        endEncoding();
        auto d = checkDict(3);
        auto output = d->toJSON();
        AssertEqual((slice)output, json);
    }

    void testDump() {
        std::string json = "{\"foo\":123,"
                           "\"\\\"ironic\\\"\":[null,false,true,-100,0,100,123.456,6.02e+23],"
                           "\"\":\"hello\\nt\\\\here\"}";
        JSONConverter j(enc);
        j.convertJSON(slice(json));
        endEncoding();
        std::string dumped = value::dump(result);
        //std::cerr << dumped;
        AssertEqual(dumped, std::string(
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

    void testConvertPeople() {
        alloc_slice input = readFile(kTestFilesDir "1000people.json");

        enc.uniqueStrings(true);

        JSONConverter jr(enc);
        jr.convertJSON(input);

#if 0
        // Dump the string table and some statistics:
        auto &strings = enc._strings;
        unsigned i = 0, totalMisses = 0, totalStrLength = 0, totalKeys = 0;
        for (auto iter = strings.begin(); iter != strings.end(); ++iter) {
            if (iter->buf) {
                auto hash = StringTable::hash(iter);
                auto shortHash = hash % strings.tableSize();
                char x[20] = "   ";
                int misses = (int)i - (int)shortHash;
                if (misses < 0)
                    misses += strings.tableSize();
                if (misses) {
                    sprintf(x, " +%d", misses);
                    totalMisses += misses;
                }
                totalStrLength += iter->size;
                if (iter.value().usedAsKey)
                    totalKeys++;
                fprintf(stderr, "\t%5X: (%08X%s) `%.*s` --> %u\n", i, hash, x, (int)iter->size, iter->buf, iter.value().offset);
            } else {
                fprintf(stderr, "\t%5X: ----\n", i);
            }
            ++i;
        }
        fprintf(stderr, "Capacity %zd, %zu occupied (%.0f%%), %zd keys, average of %.3g misses\n",
                strings.tableSize(), strings.count(),
                strings.count()/(double)strings.tableSize()*100.0,
                totalKeys,
                totalMisses / (double)strings.count());
        fprintf(stderr, "Total string size %u bytes\n", totalStrLength);
#endif

        enc.end();
        result = writer.extractOutput();

        Assert(result.buf);
        writeToFile(result, kTestFilesDir "1000people.fleece");

        fprintf(stderr, "\nJSON size: %zu bytes; Fleece size: %zu bytes (%.2f%%)\n",
                input.size, result.size, (result.size*100.0/input.size));
#ifndef NDEBUG
        fprintf(stderr, "Narrow: %u, Wide: %u (total %u)\n", enc._numNarrow, enc._numWide, enc._numNarrow+enc._numWide);
        fprintf(stderr, "Narrow count: %u, Wide count: %u (total %u)\n", enc._narrowCount, enc._wideCount, enc._narrowCount+enc._wideCount);
        fprintf(stderr, "Used %u pointers to shared strings\n", enc._numSavedStrings);
#endif
    }

    void testFindPersonByIndexUnsorted() {
        mmap_slice doc(kTestFilesDir "1000people.fleece");
        auto root = value::fromTrustedData(doc)->asArray();
        auto person = root->get(123)->asDict();
        const value *name = person->get_unsorted(slice("name"));
        std::string nameStr = (std::string)name->asString();
        AssertEqual(nameStr, std::string("Concepcion Burns"));
    }

    void testFindPersonByIndexSorted() {
        mmap_slice doc(kTestFilesDir "1000people.fleece");
        auto root = value::fromTrustedData(doc)->asArray();
        auto person = root->get(123)->asDict();
        const value *name = person->get(slice("name"));
        std::string nameStr = (std::string)name->asString();
        AssertEqual(nameStr, std::string("Concepcion Burns"));
    }

    void testFindPersonByIndexKeyed() {
        mmap_slice doc(kTestFilesDir "1000people.fleece");

        const value *nameKey = NULL;
        auto root = value::fromTrustedData(doc)->asArray();
        auto person = root->get(123)->asDict();
        for (auto i=person->begin(); i; ++i) {
            if (i.key()->asString() == slice("name")) {
                nameKey = i.key();
                break;
            }
        }
        Assert(nameKey);

        const value *name = person->get(nameKey);
        std::string nameStr = (std::string)name->asString();
        AssertEqual(nameStr, std::string("Concepcion Burns"));
    }

    CPPUNIT_TEST_SUITE( EncoderTests );
    CPPUNIT_TEST( testSpecial );
    CPPUNIT_TEST( testInts );
    CPPUNIT_TEST( testFloats );
    CPPUNIT_TEST( testStrings );
    CPPUNIT_TEST( testArrays );
    CPPUNIT_TEST( testDictionaries );
    CPPUNIT_TEST( testSharedStrings );
    CPPUNIT_TEST( testJSONStrings );
    CPPUNIT_TEST( testJSON );
    CPPUNIT_TEST( testDump );
    CPPUNIT_TEST( testConvertPeople );
    CPPUNIT_TEST( testFindPersonByIndexUnsorted );
    CPPUNIT_TEST( testFindPersonByIndexSorted );
    CPPUNIT_TEST( testFindPersonByIndexKeyed );
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(EncoderTests);

}
