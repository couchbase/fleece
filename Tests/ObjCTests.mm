//
//  ObjCTests.mm
//  Fleece
//
//  Created by Jens Alfke on 11/15/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//

#include <Foundation/Foundation.h>
#include "FleeceTests.hh"
#include "FleeceDocument.h"


class ObjCTests : public CppUnit::TestFixture {
public:

    void checkIt(id obj, const char* json) {
        Writer writer;
        Encoder enc(writer);
        enc.write(obj);
        enc.end();
        auto result = writer.extractOutput();
        auto v = value::fromData(result);
        Assert(v != NULL);
        AssertEqual(v->toJSON(), std::string(json));
        Assert([v->toNSObject() isEqual: obj]);
    }

    
    void testSpecial() {
        checkIt([NSNull null], "null");
        checkIt(@NO,  "false");
        checkIt(@YES, "true");
    }

    void testInts() {
        checkIt( @0,    "0");
        checkIt(@-1,    "-1");
        checkIt( @1234, "1234");
        checkIt(@-1234, "-1234");
        checkIt( @2047, "2047");
        checkIt(@-2048, "-2048");

        checkIt(@123456789,     "123456789");
        checkIt(@123456789012,  "123456789012");
        checkIt(@(INT64_MAX),   "9223372036854775807");
        checkIt(@(UINT64_MAX),  "18446744073709551615");
    }

    void testFloats() {
        checkIt(@0.5,  "0.5");
        checkIt(@-0.5, "-0.5");
        checkIt(@((float)M_PI), "3.14159");
        checkIt(@((double)M_PI), "3.141592653589793");
    }

    void testStrings() {
        checkIt(@"",                    "\"\"");
        checkIt(@"!",                   "\"!\"");
        checkIt(@"müßchop",             "\"müßchop\"");

        checkIt(@"howdy",               "\"howdy\"");
        checkIt(@"\"ironic\"",          "\"\\\"ironic\\\"\"");
        checkIt(@"an \"ironic\" twist", "\"an \\\"ironic\\\" twist\"");
        checkIt(@"\\foo\\",             "\"\\\\foo\\\\\"");
        checkIt(@"\tline1\nline2\t",    "\"\\tline1\\nline2\\t\"");
        checkIt(@"line1\01\02line2",    "\"line1\\u0001\\u0002line2\"");
    }

    void testArrays() {
        checkIt(@[], "[]");
        checkIt(@[@123], "[123]");
        checkIt(@[@123, @"howdy", @1234.5678], "[123,\"howdy\",1234.5678]");
        checkIt(@[@[@[],@[]],@[]], "[[[],[]],[]]");
        checkIt(@[@"flumpety", @"flumpety", @"flumpety"], "[\"flumpety\",\"flumpety\",\"flumpety\"]");
    }

    void testDictionaries() {
        checkIt(@{}, "{}");
        checkIt(@{@"n":@123}, "{\"n\":123}");
        checkIt(@{@"n":@123, @"slang":@"howdy", @"long":@1234.5678},
                "{\"long\":1234.5678,\"n\":123,\"slang\":\"howdy\"}");
        checkIt(@{@"a":@{@"a":@{},@"b":@{}},@"c":@{}},
                "{\"a\":{\"a\":{},\"b\":{}},\"c\":{}}");
        checkIt(@{@"a":@"flumpety", @"b":@"flumpety", @"c":@"flumpety"},
                "{\"a\":\"flumpety\",\"b\":\"flumpety\",\"c\":\"flumpety\"}");
    }

    void testPerfParse1000PeopleNS() {
        @autoreleasepool {
            const int kSamples = 50;
            double total = 0, minTime = 1e99, maxTime = -1;
            NSData *data = [NSData dataWithContentsOfFile: @kTestFilesDir "1000people.json"];

            fprintf(stderr, "Parsing JSON to NSObjects (ms):");
            for (int i = 0; i < kSamples; i++) {
                Stopwatch st;

                @autoreleasepool {
                    id j = [NSJSONSerialization JSONObjectWithData: data options: 0 error: NULL];
                    Assert(j);
                }

                double elapsed = st.elapsedMS();
                fprintf(stderr, " %g", elapsed);
                total += elapsed;
                minTime = std::min(minTime, elapsed);
                maxTime = std::max(maxTime, elapsed);
            }
            fprintf(stderr, "\nAverage time is %g ms\n", (total - minTime - maxTime)/(kSamples-2));
        }
    }

    void testFleeceLazyDict() {
        @autoreleasepool {
            NSData *data = [NSData dataWithContentsOfFile: @kTestFilesDir "1000people.fleece"];
            NSArray *people = [FleeceDocument objectWithFleeceData: data trusted: YES];
            Assert([people isKindOfClass: [NSArray class]]);

            int i = 0;
            for (NSDictionary *person in people) {
                Assert(person[@"name"] != nil);
                //NSLog(@"#%3d: count=%2lu, name=`%@`", i, person.count, person[@"name"]);
                //NSLog(@"%@", person);
                i++;
            }
            AssertEqual(i, 1000);
        }
    }

    void testPerfReadNameNS() {
        @autoreleasepool {
            NSData *data = [NSData dataWithContentsOfFile: @kTestFilesDir "1person.json"];
            Stopwatch st;
            for (int j = 0; j < 1e5; j++) {
                @autoreleasepool {
                    NSDictionary *person = [NSJSONSerialization JSONObjectWithData: data options: 0 error: NULL];
                    if (person[@"name"] == nil)
                        abort();
                }
            }
            fprintf(stderr, "Getting name (NS) took %g µs\n", st.elapsedMS()/1e5*1000);
        }
    }

    void testPerfReadName() {
        @autoreleasepool {
            NSData *data = [NSData dataWithContentsOfFile: @kTestFilesDir "1person.fleece"];
            Stopwatch st;
            for (int j = 0; j < 1e5; j++) {
                @autoreleasepool {
                    NSDictionary *person = [FleeceDocument objectWithFleeceData: data trusted: YES];
                    if (person[@"name"] == nil)
                        abort();
                }
            }
            fprintf(stderr, "Getting name (Fleece) took %g µs\n", st.elapsedMS()/1e5*1000);
        }
    }
    
    void testPerfReadNamesNS() {
        @autoreleasepool {
            NSData *data = [NSData dataWithContentsOfFile: @kTestFilesDir "1000people.json"];
            NSArray *people = [NSJSONSerialization JSONObjectWithData: data options: 0 error: NULL];
            Assert([people isKindOfClass: [NSArray class]]);

            Stopwatch st;
            for (int j = 0; j < 10000; j++) {
                int i = 0;
                for (NSDictionary *person in people) {
                    if (person[@"name"] == nil)
                        abort();
                    i++;
                }
            }
            fprintf(stderr, "Iterating people (NS) took %g ms\n", st.elapsedMS()/10000);
        }
    }

    void testPerfReadNames() {
        @autoreleasepool {
            NSData *data = [NSData dataWithContentsOfFile: @kTestFilesDir "1000people.fleece"];
            NSArray *people = [FleeceDocument objectWithFleeceData: data trusted: YES];
            Assert([people isKindOfClass: [NSArray class]]);

            Stopwatch st;
            for (int j = 0; j < 10000; j++) {
                int i = 0;
                for (NSDictionary *person in people) {
                    if (person[@"name"] == nil)
                        abort();
                    i++;
                }
            }
            fprintf(stderr, "Iterating people (Fleece) took %g ms\n", st.elapsedMS()/10000);
        }
    }
    
    CPPUNIT_TEST_SUITE( ObjCTests );
    CPPUNIT_TEST( testSpecial );
    CPPUNIT_TEST( testInts );
    CPPUNIT_TEST( testFloats );
    CPPUNIT_TEST( testStrings );
    CPPUNIT_TEST( testArrays );
    CPPUNIT_TEST( testDictionaries );
    CPPUNIT_TEST( testFleeceLazyDict );
#ifdef NDEBUG
//    CPPUNIT_TEST( testPerfParse1000PeopleNS );
    CPPUNIT_TEST( testPerfReadName );
    CPPUNIT_TEST( testPerfReadNameNS );
    CPPUNIT_TEST( testPerfReadNames );
    CPPUNIT_TEST( testPerfReadNamesNS );
#endif
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(ObjCTests);
