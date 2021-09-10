//
// ObjCTests.mm
//
// Copyright 2015-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include <Foundation/Foundation.h>
#include "FleeceTests.hh"
#include "FleeceImpl.hh"

using namespace fleece::impl;

static void checkIt(id obj, const char* json) {
    @autoreleasepool {
        Encoder enc;
        enc.writeObjC(obj);
        enc.end();
        auto result = enc.finish();
        auto v = Value::fromData(result);
        REQUIRE(v != nullptr);
        REQUIRE(v->toJSON() == alloc_slice(json));
        REQUIRE([v->toNSObject() isEqual: obj]);
    }
}


TEST_CASE("Slice NSData") {
    @autoreleasepool {
        slice str = "Hi, I am a string";
        NSData *data;
        {
            alloc_slice s(str);
            data = s.uncopiedNSData();
            // `s` is released, but `data` should have its own reference to the heap block...
        }
        CHECK(data.length == str.size);
        CHECK(slice(data) == str);
    }
}


TEST_CASE("Obj-C Special", "[Encoder]") {
    checkIt([NSNull null], "null");
    checkIt(@NO,  "false");
    checkIt(@YES, "true");
}

TEST_CASE("Obj-C Ints", "[Encoder]") {
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

TEST_CASE("Obj-C Floats", "[Encoder]") {
    checkIt(@0.5,  "0.5");
    checkIt(@-0.5, "-0.5");
    checkIt(@((float)M_PI), "3.1415927");
    checkIt(@((double)M_PI), "3.141592653589793");
}

TEST_CASE("Obj-C Strings", "[Encoder]") {
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

TEST_CASE("Obj-C Arrays", "[Encoder]") {
    checkIt(@[], "[]");
    checkIt(@[@123], "[123]");
    checkIt(@[@123, @"howdy", @1234.5678], "[123,\"howdy\",1234.5678]");
    checkIt(@[@[@[],@[]],@[]], "[[[],[]],[]]");
    checkIt(@[@"flumpety", @"flumpety", @"flumpety"], "[\"flumpety\",\"flumpety\",\"flumpety\"]");
}

TEST_CASE("Obj-C Dictionaries", "[Encoder]") {
    checkIt(@{}, "{}");
    checkIt(@{@"n":@123}, "{\"n\":123}");
    checkIt(@{@"n":@123, @"slang":@"howdy", @"long":@1234.5678},
            "{\"long\":1234.5678,\"n\":123,\"slang\":\"howdy\"}");
    checkIt(@{@"a":@{@"a":@{},@"b":@{}},@"c":@{}},
            "{\"a\":{\"a\":{},\"b\":{}},\"c\":{}}");
    checkIt(@{@"a":@"flumpety", @"b":@"flumpety", @"c":@"flumpety"},
            "{\"a\":\"flumpety\",\"b\":\"flumpety\",\"c\":\"flumpety\"}");
}

TEST_CASE("Obj-C PerfParse1000PeopleNS", "[.Perf]") {
    @autoreleasepool {
        const int kSamples = 50;
        NSData *data = [NSData dataWithContentsOfFile: @kTestFilesDir "1000people.json"];
        REQUIRE(data);

        Benchmark bench;

        fprintf(stderr, "Parsing JSON to NSObjects (ms):");
        for (int i = 0; i < kSamples; i++) {
            bench.start();

            @autoreleasepool {
                id j = [NSJSONSerialization JSONObjectWithData: data options: 0 error: nullptr];
                REQUIRE(j);
            }

            bench.stop();

            usleep(100);
        }
        bench.printReport();
    }
}

TEST_CASE("Obj-C PerfFindPersonByIndexNS", "[.Perf]") {
    @autoreleasepool {
        NSData *data = [NSData dataWithContentsOfFile: @kTestFilesDir "1000people.json"];
        REQUIRE(data);
        NSArray *people = [NSJSONSerialization JSONObjectWithData: data options: 0 error: nullptr];
        REQUIRE([people isKindOfClass: [NSArray class]]);

        int kSamples = 500;
        int kIterations = 10000;
        Benchmark bench;
        fprintf(stderr, "Looking up one value (NS)...\n");

        for (int i = 0; i < kSamples; i++) {
            @autoreleasepool {
                bench.start();
                for (int j = 0; j < kIterations; j++) {
                    // Check data types, since the Fleece version does so too
                    if (![people isKindOfClass: [NSArray class]])
                        people = nil;
                    auto person = (NSDictionary*)people[123];
                    if (![person isKindOfClass: [NSDictionary class]])
                        person = nil;
                    auto name = (NSString*)person[@"name"];
                    if (![name isKindOfClass: [NSString class]])
                        name = nil;
                    REQUIRE([name isEqualToString: @"Concepcion Burns"]);
                }
                bench.stop();
            }
        }
        bench.printReport(kIterations);
    }
}

TEST_CASE("Obj-C PerfReadNameNS", "[.Perf]") {
    @autoreleasepool {
        NSData *data = [NSData dataWithContentsOfFile: @kTestFilesDir "1person.json"];
        REQUIRE(data);
        Stopwatch st;
        for (int j = 0; j < 1e5; j++) {
            @autoreleasepool {
                NSDictionary *person = [NSJSONSerialization JSONObjectWithData: data options: 0 error: nullptr];
                if (person[@"name"] == nil)
                    abort();
            }
        }
        fprintf(stderr, "Getting name (NS) took %g µs\n", st.elapsedMS()/1e5*1000);
    }
}

TEST_CASE("Obj-C PerfReadNamesNS", "[.Perf]") {
    @autoreleasepool {
        NSData *data = [NSData dataWithContentsOfFile: @kTestFilesDir "1000people.json"];
        REQUIRE(data);
        NSArray *people = [NSJSONSerialization JSONObjectWithData: data options: 0 error: nullptr];
        REQUIRE([people isKindOfClass: [NSArray class]]);

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

TEST_CASE("Obj-C PerfSerialize", "[.Perf]") {
    @autoreleasepool {
        NSData *data = [NSData dataWithContentsOfFile: @kTestFilesDir "1000people.json"];
        REQUIRE(data);
        NSArray *people = [NSJSONSerialization JSONObjectWithData: data options: 0 error: nullptr];
        REQUIRE([people isKindOfClass: [NSArray class]]);

        size_t totalSize = 0;
        Benchmark b;

        int rep;
        for (rep = 0; rep < 1000; ++rep) {
            b.start();
            @autoreleasepool {
#if 1
                Encoder enc;
                for (NSDictionary *person in people) {
                    enc.writeObjC(person);
                    auto d = enc.finish();
                    totalSize += d.size;
                    enc.reset();
                }
#else
                Encoder enc;
                enc.write(people);
                auto d = enc.finish();
                totalSize += d.size;
#endif
            }
            b.stop();
        }
        b.printReport(1, "dict");
        // 10/30/16: 12.4us/dict (using isKindOf)
        //           11.3us/dict (using category)
        fprintf(stderr, "Total size = %zu bytes\n", totalSize);
    }
}
