//
//  EncoderTests.cpp
//  Fleece
//
//  Created by Jens Alfke on 11/14/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//

#include "FleeceTests.hh"


class EncoderTests : public CppUnit::TestFixture {
public:
    EncoderTests()
    :enc(writer)
    { }

    Writer writer;
    encoder enc;
    alloc_slice result;

    void endEncoding() {
        enc.end();
        result = alloc_slice::adopt(writer.extractOutput());
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
        enc.reset();
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
        Assert(!v->isInteger());
        Assert(!v->isDouble());
        AssertEqual(v->asInt(), (int64_t)round(f));
        AssertEqual(v->asFloat(), f);
        AssertEqual(v->asDouble(), (double)f);
    }

    void checkReadDouble(double f) {
        auto v = value::fromData(result);
        Assert(v != NULL);
        Assert(v->type() == kNumber);
        Assert(!v->isInteger());
        Assert(v->isDouble());
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

        enc.writeInt( 2048);    checkOutput("1100 0800");    checkRead(2048);
        enc.writeInt(-2049);    checkOutput("11FF F700");    checkRead(-2049);
        enc.writeInt(0x223344); checkOutput("1244 3322");    checkRead(0x223344);
        enc.writeInt(0x11223344556677);    checkOutput("1677 6655 4433 2211");
        checkRead(0x11223344556677);
        enc.writeInt(0x1122334455667788);  checkOutput("1788 7766 5544 3322 1100");
        checkRead(0x1122334455667788);
        enc.writeInt(-0x1122334455667788); checkOutput("1778 8899 AABB CCDD EE00");
        checkRead(-0x1122334455667788);
        enc.writeUInt(0xCCBBAA9988776655); checkOutput("1F55 6677 8899 AABB CC00");
        checkReadU(0xCCBBAA9988776655);
        enc.writeUInt(UINT64_MAX);         checkOutput("1FFF FFFF FFFF FFFF FF00");
        checkReadU(UINT64_MAX);
    }

    void testFloats() {
        enc.writeFloat( 0.5);   checkOutput("2400 0000 003F");           checkReadFloat( 0.5);
        enc.writeFloat(-0.5);   checkOutput("2400 0000 00BF");           checkReadFloat(-0.5);
        enc.writeFloat(M_PI);   checkOutput("2400 DB0F 4940");           checkReadFloat(M_PI);
        enc.writeDouble(M_PI);  checkOutput("2800 182D 4454 FB21 0940"); checkReadDouble(M_PI);
    }

    void testStrings() {
        enc.writeString("");    checkOutput("4000");          checkReadString("");
        enc.writeString("a");   checkOutput("4161");        checkReadString("a");
        enc.writeString("ab");  checkOutput("4261 62");     checkReadString("ab");
        enc.writeString("abcdefghijklmn");  checkOutput("4E61 6263 6465 6667 6869 6A6B 6C6D 6E");
        checkReadString("abcdefghijklmn");
        enc.writeString("abcdefghijklmno"); checkOutput("4F0F 6162 6364 6566 6768 696A 6B6C 6D6E 6F");
        checkReadString("abcdefghijklmno");
        enc.writeString("abcdefghijklmnop"); checkOutput("4F10 6162 6364 6566 6768 696A 6B6C 6D6E 6F70");
        checkReadString("abcdefghijklmnop");

        enc.writeString("müßchop"); checkOutput("496D C3BC C39F 6368 6F70");
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

    void _testArrays(bool wide) {
        {
            encoder array = enc.writeArray(0, wide);
            array.end();
            checkOutput(wide ? "6800" : "6000");
            checkArray(0);
        }
        {
            encoder array = enc.writeArray(1, wide);
            array.writeNull();
            array.end();
            checkOutput(wide ? "6801 3000 0000" : "6001 3000");
            auto a = checkArray(1);
            auto v = a->get(0);
            Assert(v);
            AssertEqual(v->type(), kNull);
        }
        {
            encoder array = enc.writeArray(2, wide);
            array.writeString("a");
            array.writeString("hello");
            array.end();
            checkOutput(wide ? "6802 4161 0000 8000 0002 4568 656C 6C6F"
                             : "6002 4161 8001 4568 656C 6C6F");
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

            AssertEqual(a->toJSON(), std::string("[\"a\",\"hello\"]"));
        }
        {
            // Strings that can be inlined in a wide array:
            encoder array = enc.writeArray(2, wide);
            array.writeString("ab");
            array.writeString("cde");
            array.end();
            checkOutput(wide ? "6802 4261 6200 4363 6465"
                             : "6002 8002 8003 4261 6200 4363 6465");
        }
    }

    void testArrays()       {_testArrays(false);}
    void testWideArrays()   {_testArrays(true);}

    void testDictionaries() {
        {
            encoder dict = enc.writeDict(0);
            dict.end();
            checkOutput("7000");
            checkDict(0);
        }
        {
            encoder dict = enc.writeDict(1);
            dict.writeKey("foo");
            dict.writeInt(42);
            dict.end();
            checkOutput("7001 8002 002A 4366 6F6F");
            auto d = checkDict(1);
            auto v = d->get(slice("foo"));
            Assert(v);
            AssertEqual(v->asInt(), 42ll);
            AssertEqual(d->get(slice("barrr")), (const value*)NULL);
            AssertEqual(d->toJSON(), std::string("{\"foo\":42}"));
        }
    }

    void testSharedStrings() {
        encoder array = enc.writeArray(4);
        array.writeString("a");
        array.writeString("hello");
        array.writeString("a");
        array.writeString("hello");
        array.end();
        checkOutput("6004 4161 8003 4161 8001 4568 656C 6C6F");
        auto a = checkArray(4);
        AssertEqual(a->toJSON(), std::string("[\"a\",\"hello\",\"a\",\"hello\"]"));
    }

    CPPUNIT_TEST_SUITE( EncoderTests );
    CPPUNIT_TEST( testSpecial );
    CPPUNIT_TEST( testInts );
    CPPUNIT_TEST( testFloats );
    CPPUNIT_TEST( testStrings );
    CPPUNIT_TEST( testArrays );
    CPPUNIT_TEST( testWideArrays );
    CPPUNIT_TEST( testDictionaries );
    CPPUNIT_TEST( testSharedStrings );
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(EncoderTests);
