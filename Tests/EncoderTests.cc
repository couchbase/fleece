//
//  EncoderTests.cpp
//  Fleece
//
//  Created by Jens Alfke on 11/14/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//

#include "FleeceTests.hh"
#include "JSONConverter.hh"
#include "KeyTree.hh"
#include "jsonsl.h"
#include "mn_wordlist.h"

namespace fleece {

class EncoderTests : public CppUnit::TestFixture {
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
        result = enc.extractOutput();
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
        auto v = Value::fromData(result);
        Assert(v != NULL);
        Assert(v->type() == kBoolean);
        AssertEqual(v->asBool(), b);
        AssertEqual(v->asInt(), (int64_t)b);
    }

    void checkRead(int64_t i) {
        auto v = Value::fromData(result);
        Assert(v != NULL);
        Assert(v->type() == kNumber);
        Assert(v->isInteger());
        Assert(!v->isUnsigned());
        AssertEqual(v->asInt(), i);
        AssertEqual(v->asDouble(), (double)i);
    }

    void checkReadU(uint64_t i) {
        auto v = Value::fromData(result);
        Assert(v->type() == kNumber);
        Assert(v->isInteger());
        Assert(v->isUnsigned());
        AssertEqual(v->asUnsigned(), i);
        AssertEqual(v->asDouble(), (double)i);
    }

    void checkReadFloat(float f) {
        auto v = Value::fromData(result);
        Assert(v != NULL);
        Assert(v->type() == kNumber);
        Assert(!v->isDouble());
        AssertEqual(v->asInt(), (int64_t)round(f));
        AssertEqual(v->asFloat(), f);
        AssertEqual(v->asDouble(), (double)f);
    }

    void checkReadDouble(double f) {
        auto v = Value::fromData(result);
        Assert(v != NULL);
        Assert(v->type() == kNumber);
        AssertEqual(v->asInt(), (int64_t)round(f));
        AssertEqual(v->asDouble(), f);
        AssertEqual(v->asFloat(), (float)f);
    }

    void checkReadString(const char *str) {
        auto v = Value::fromData(result);
        Assert(v != NULL);
        Assert(v->type() == kString);
        AssertEqual(v->asString(), slice(str, strlen(str)));
    }

    const Array* checkArray(uint32_t count) {
        auto v = Value::fromData(result);
        Assert(v != NULL);
        Assert(v->type() == kArray);
        auto a = v->asArray();
        Assert(a != NULL);
        AssertEqual(a->count(), count);
        return a;
    }

    const Dict* checkDict(uint32_t count) {
        auto v = Value::fromData(result);
        Assert(v != NULL);
        Assert(v->type() == kDict);
        auto d = v->asDict();
        Assert(d != NULL);
        AssertEqual(d->count(), count);
        return d;
    }

#pragma mark - TESTS

    void testEmpty() {
        Assert(enc.isEmpty());
        enc.beginArray();
        Assert(!enc.isEmpty());
        enc.endArray();

        Encoder enc2;
        Assert(enc2.isEmpty());
        enc2 << 17;
        Assert(!enc2.isEmpty());

        enc2.reset();
        Assert(enc2.isEmpty());
    }

    void testPointer() {
        uint8_t data[2] = {0x80, 0x02};
        auto v = (const Value*)data;
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
            Array::iterator iter(a);
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

    void testLongArrays() {
        testArrayOfLength(0x7FE);
        testArrayOfLength(0x7FF);
        testArrayOfLength(0x800);
        testArrayOfLength(0x801);
        testArrayOfLength(0xFFFF);
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
            Assert(v);
            AssertEqual(v->type(), kNumber);
            AssertEqual(v->asUnsigned(), (uint64_t)i);
        }
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
            AssertEqual(d->get(slice("barrr")), (const Value*)NULL);
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
            AssertEqual(d->get(slice("barrr")), (const Value*)NULL);
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

    void testJSONBinary() {
        enc.beginArray();
        enc.writeData(slice("not-really-binary"));
        enc.endArray();
        endEncoding();
        auto json = Value::fromData(result)->toJSON();
        AssertEqual(json, alloc_slice("[\"bm90LXJlYWxseS1iaW5hcnk=\"]"));

        Writer w;
        w.writeDecodedBase64(slice("bm90LXJlYWxseS1iaW5hcnk="));
        AssertEqual(w.extractOutput(), alloc_slice("not-really-binary"));
    }

    void testDump() {
        std::string json = "{\"foo\":123,"
                           "\"\\\"ironic\\\"\":[null,false,true,-100,0,100,123.456,6.02e+23],"
                           "\"\":\"hello\\nt\\\\here\"}";
        JSONConverter j(enc);
        j.convertJSON(slice(json));
        endEncoding();
        std::string dumped = Value::dump(result);
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
                if (iter.Value().usedAsKey)
                    totalKeys++;
                fprintf(stderr, "\t%5X: (%08X%s) `%.*s` --> %u\n", i, hash, x, (int)iter->size, iter->buf, iter.Value().offset);
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
        result = enc.extractOutput();

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
        auto root = Value::fromTrustedData(doc)->asArray();
        auto person = root->get(123)->asDict();
        const Value *name = person->get_unsorted(slice("name"));
        std::string nameStr = (std::string)name->asString();
        AssertEqual(nameStr, std::string("Concepcion Burns"));
    }

    void testFindPersonByIndexSorted() {
        mmap_slice doc(kTestFilesDir "1000people.fleece");
        auto root = Value::fromTrustedData(doc)->asArray();
        auto person = root->get(123)->asDict();
        const Value *name = person->get(slice("name"));
        std::string nameStr = (std::string)name->asString();
        AssertEqual(nameStr, std::string("Concepcion Burns"));
    }

    void testFindPersonByIndexKeyed() {
        {
            Dict::key nameKey(slice("name"), true);

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
            Assert(smol->get(2)->asDict()->get(nameKey) == NULL);

            lookupNamesWithKeys(smol->get(0)->asDict(), "Concepcion Burns", false);
            lookupNamesWithKeys(smol->get(1)->asDict(), "Carmen Miranda", false);
            lookupNamesWithKeys(smol->get(2)->asDict(), NULL, false);
        }
        {
            // Now try a wide Dict:
            Dict::key nameKey(slice("name"), true);

            mmap_slice doc(kTestFilesDir "1000people.fleece");
            auto root = Value::fromTrustedData(doc)->asArray();
            auto person = root->get(123)->asDict();
            lookupNameWithKey(person, nameKey, "Concepcion Burns");
            lookupNamesWithKeys(person, "Concepcion Burns", -1);

            person = root->get(3)->asDict();
            lookupNameWithKey(person, nameKey, "Isabella Compton");
            lookupNamesWithKeys(person, "Isabella Compton", -1);
        }
    }

    void lookupNameWithKey(const Dict* person, Dict::key &nameKey, std::string expectedName) {
        Assert(person);
        const Value *name = person->get(nameKey);
        Assert(name);
        std::string nameStr = (std::string)name->asString();
        AssertEqual(nameStr, expectedName);
        Assert(nameKey.asValue() != NULL);
        AssertEqual(nameKey.asValue()->asString(), slice("name"));

        // Second lookup (using cache)
        name = person->get(nameKey);
        Assert(name);
        nameStr = (std::string)name->asString();
        AssertEqual(nameStr, expectedName);
    }

    void lookupNamesWithKeys(const Dict* person, const char *expectedName, int expectedX) {
        Dict::key nameKey(slice("name"));
        Dict::key xKey(slice("x"));
        Dict::key keys[2] = {nameKey, xKey};
        const Value* values[2];
        auto found = person->get(keys, values, 2);
        size_t expectedFound = (expectedName != NULL) + (expectedX >= false);
        AssertEqual(found, expectedFound);
        if (expectedName)
            AssertEqual(values[0]->asString(), slice(expectedName));
        else
            Assert(values[0] == NULL);
        if (expectedX >= false) {
            AssertEqual(values[1]->type(), kBoolean);
            AssertEqual(values[1]->asBool(), (bool)expectedX);
        } else {
            Assert(values[1] == NULL);
        }
    }

    void testLookupManyKeys() {
        mmap_slice doc(kTestFilesDir "1person.fleece");
        auto person = Value::fromTrustedData(doc)->asDict();

        Dict::key keys[] = {
            Dict::key(slice("about")),
            Dict::key(slice("age")),
            Dict::key(slice("balance")),
            Dict::key(slice("guid")),
            Dict::key(slice("isActive")),
            Dict::key(slice("latitude")),
            Dict::key(slice("longitude")),
            Dict::key(slice("name")),
            Dict::key(slice("registered")),
            Dict::key(slice("tags")),

//            Dict::key(slice("jUNK")),
//            Dict::key(slice("abut")),
//            Dict::key(slice("crud")),
//            Dict::key(slice("lowrider")),
//            Dict::key(slice("ocarina")),
//            Dict::key(slice("time")),
//            Dict::key(slice("tangle")),
//            Dict::key(slice("b")),
//            Dict::key(slice("f")),
//            Dict::key(slice("g")),
//            Dict::key(slice("m")),
//            Dict::key(slice("n")),
//            Dict::key(slice("z")),
        };
        unsigned nKeys = sizeof(keys) / sizeof(keys[0]);
        Dict::sortKeys(keys, nKeys);

#ifndef NDEBUG
        internal::gTotalComparisons = 0;
#endif
        const Value* values[nKeys];
        AssertEqual(person->get(keys, values, nKeys), 10ul);
#ifndef NDEBUG
        fprintf(stderr, "... that took %u comparisons, or %.1f/key\n",
                internal::gTotalComparisons, internal::gTotalComparisons/(float)nKeys);
        internal::gTotalComparisons = 0;
#endif
        for (unsigned i = 0; i < nKeys; ++i)
            AssertEqual(values[i], person->get(keys[i]));

        AssertEqual(person->get(keys, values, nKeys), 10ul);
#ifndef NDEBUG
        fprintf(stderr, "... second pass took %u comparisons, or %.1f/key\n",
                internal::gTotalComparisons, internal::gTotalComparisons/(float)nKeys);
#endif
    }

#pragma mark - KEY TREE:

    void testKeyTree() {
        char eeeeeeee[1024] = "";
        memset(&eeeeeeee[0], 'e', sizeof(eeeeeeee)-1);

#if 1
        size_t n = sizeof(mn_words)/sizeof(char*);
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
        std::cerr << "\n" << sliceToHexDump(output, 32);
        std::cerr << "Size = " << output.size << "; that's " << output.size-totalLen << " bytes overhead for " << n << " strings, i.e." << (output.size-totalLen)/(double)n << " bytes/string.\n";

        std::vector<bool> ids(n+1);
        for (size_t i = 0; i < n; ++i) {
            std::cerr << "Checking '" << rawStrings[i] << "' ... ";
            unsigned id = keys[strings[i]];
            std::cerr << id << " ... ";
            Assert(id);
            Assert(!ids[id]);
            ids[id] = true;

            slice lookup = keys[id];
            std::cerr << lookup << "\n";
            Assert(lookup.buf);
            AssertEqual(lookup, strings[i]);
        }

        Assert(keys[slice("")] == 0);
        Assert(keys[slice("foo")] == 0);
        Assert(keys[slice("~")] == 0);
        Assert(keys[slice("whiske")] == 0);
        Assert(keys[slice("whiskex")] == 0);
        Assert(keys[slice("whiskez")] == 0);

        Assert(keys[0].buf == NULL);
        Assert(keys[(unsigned)n+1].buf == NULL);
        Assert(keys[(unsigned)n+2].buf == NULL);
        Assert(keys[(unsigned)n+28].buf == NULL);
        Assert(keys[(unsigned)9999].buf == NULL);
    }

    CPPUNIT_TEST_SUITE( EncoderTests );
    CPPUNIT_TEST( testEmpty );
    CPPUNIT_TEST( testSpecial );
    CPPUNIT_TEST( testInts );
    CPPUNIT_TEST( testFloats );
    CPPUNIT_TEST( testStrings );
    CPPUNIT_TEST( testArrays );
    CPPUNIT_TEST( testLongArrays );
    CPPUNIT_TEST( testDictionaries );
    CPPUNIT_TEST( testSharedStrings );
    CPPUNIT_TEST( testJSONStrings );
    CPPUNIT_TEST( testJSON );
    CPPUNIT_TEST( testJSONBinary );
    CPPUNIT_TEST( testDump );
    CPPUNIT_TEST( testConvertPeople );
    CPPUNIT_TEST( testFindPersonByIndexUnsorted );
    CPPUNIT_TEST( testFindPersonByIndexSorted );
    CPPUNIT_TEST( testFindPersonByIndexKeyed );
    CPPUNIT_TEST( testLookupManyKeys );
    CPPUNIT_TEST( testKeyTree );
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(EncoderTests);

}
