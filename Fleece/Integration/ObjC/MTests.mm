//
// MutableTests.m
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include <Foundation/Foundation.h>
#include "FleeceTests.hh"
#include "MArray+ObjC.h"
#include "MDict+ObjC.h"
#include "MRoot.hh"
#include "FleeceDocument.h"
#include <iostream>

using namespace fleece;


static alloc_slice encode(id obj) {
    Encoder enc;
    enc << obj;
    alloc_slice result( enc.finish() );
    if (!result)
        INFO("Fleece encoder error " << enc.error() << ": " << enc.errorMessage());
    REQUIRE(result);
    return result;
}


static alloc_slice encode(const MRoot<id> &val) {
    Encoder enc;
    val.encodeTo(enc);
    return enc.finish();
}


static NSArray* sortedKeys(NSDictionary* dict) {
    return [dict.allKeys sortedArrayUsingSelector: @selector(compare:)];
}


static std::string fleece2JSON(alloc_slice fleece) {
    auto v = ValueFromData(fleece);
    if (!v)
        return "INVALID_FLEECE";
    return alloc_slice(v.toJSON5()).asString();
}


static void verifyDictIterator(NSDictionary *dict) {
    __block NSUInteger count = 0;
    NSMutableSet* keys = [NSMutableSet set];
    [dict enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
        ++count;
        CHECK(key);
        CHECK(obj);
        CHECK([obj isEqual: dict[key]]);
        [keys addObject: key];
    }];
    CHECK(count == dict.count);
    CHECK(keys.count == dict.count);
}


TEST_CASE("MValue", "[Mutable]") {
    @autoreleasepool {
        MValue<id> val(@"hi");
        REQUIRE([val.asNative(nullptr) isEqual: @"hi"]);
        REQUIRE(val.value() == nullptr);
    }
}

TEST_CASE("MDict", "[Mutable]") {
    @autoreleasepool {
        auto data = encode(@{@"greeting": @"hi",
                             @"array":    @[@"boo", @NO],
                             @"dict":     @{@"melt": @32, @"boil": @212}});
        MRoot<id> root(data);
        CHECK(!root.isMutated());
        NSMutableDictionary* dict = root.asNative();
        NSLog(@"FleeceDict = %@", dict);
        CHECK(([sortedKeys(dict) isEqual: @[@"array", @"dict", @"greeting"]]));
        CHECK([dict[@"greeting"] isEqual: @"hi"]);
        CHECK(dict[@"x"] == nil);

        NSMutableDictionary* nested = dict[@"dict"];
        CHECK(([sortedKeys(nested) isEqual: @[@"boil", @"melt"]]));
        CHECK(([nested isEqual: @{@"melt": @32, @"boil": @212}]));
        CHECK([nested[@"melt"] isEqual: @32]);
        CHECK([nested[@"boil"] isEqual: @212]);
        CHECK(nested[@"freeze"] == nil);
        CHECK(([nested isEqual: @{@"melt": @32, @"boil": @212}]));
        CHECK(!root.isMutated());

        verifyDictIterator(dict);

        nested[@"freeze"] = @[@32, @"Fahrenheit"];
        CHECK(root.isMutated());
        [nested removeObjectForKey: @"melt"];
        CHECK(([nested isEqual: @{@"freeze": @[@32, @"Fahrenheit"], @"boil": @212}]));

        verifyDictIterator(dict);

        CHECK(fleece2JSON(encode(dict)) == "{array:[\"boo\",false],dict:{boil:212,freeze:[32,\"Fahrenheit\"]},greeting:\"hi\"}");
        CHECK(fleece2JSON(encode(root)) == "{array:[\"boo\",false],dict:{boil:212,freeze:[32,\"Fahrenheit\"]},greeting:\"hi\"}");

        alloc_slice newData = root.encode();

        // Delta encoding:
        alloc_slice delta = root.amend(true);
        REQUIRE(delta.buf != nullptr);
        CHECK(delta.size == 52); // may change

        alloc_slice combinedData(data);
        combinedData.append(delta);
        Dict newDict = ValueFromData(combinedData).asDict();
        std::cerr << "\nContents:      " << alloc_slice(newDict.toJSON()).asString() << "\n";
        std::cerr <<   "Old:           " << data.size << " bytes: " << data << "\n\n";
        std::cerr <<   "New:           " << newData.size << " bytes: " << newData << "\n\n";
        std::cerr <<   "Delta:         " << delta.size << " bytes: " << delta << "\n\n";

        alloc_slice dump(FLData_Dump(combinedData));
        std::cerr << dump.asString();
    }
#if DEBUG
    CHECK(MContext::gInstanceCount == 0);
#endif
}


TEST_CASE("MArray", "[Mutable]") {
    @autoreleasepool {
        auto data = encode(@[@"hi", @[@"boo", @NO], @42]);
        MRoot<id> root(data);
        CHECK(!root.isMutated());
        NSMutableArray* array = root.asNative();
        NSLog(@"FleeceArray = %@", array);
        CHECK([array[0] isEqual: @"hi"]);
        CHECK([array[2] isEqual: @42]);
        CHECK(([array[1] isEqual: @[@"boo", @NO]]));

        array[0] = @[@(3.14), @(2.17)];
        [array insertObject: @"NEW" atIndex:2];
        NSLog(@"Array = %@", array);
        CHECK(([array isEqual: @[@[@(3.14), @(2.17)], @[@"boo", @NO], @"NEW", @42]]));

        NSMutableArray* nested = array[1];
        CHECK([nested isKindOfClass: [NSMutableArray class]]);
        nested[1] = @YES;

        CHECK(fleece2JSON(encode(array)) == "[[3.14,2.17],[\"boo\",true],\"NEW\",42]");
        CHECK(fleece2JSON(encode(root))   == "[[3.14,2.17],[\"boo\",true],\"NEW\",42]");
    }
#if DEBUG
    CHECK(MContext::gInstanceCount == 0);
#endif
}


TEST_CASE("MArray iteration", "[Mutable]") {
    @autoreleasepool {
        NSMutableArray *orig = [NSMutableArray new];
        for (NSUInteger i = 0; i < 100; i++)
            [orig addObject: [NSString stringWithFormat: @"This is item number %zu", i]];
        auto data = encode(orig);
        MRoot<id> root(data);
        NSMutableArray* array = root.asNative();

        NSUInteger i = 0;
        for (id o in array) {
            NSLog(@"item #%zu: %@", i, o);
            CHECK([o isEqual: orig[i]]);
            ++i;
        }
    }
#if DEBUG
    CHECK(MContext::gInstanceCount == 0);
#endif
}


TEST_CASE("MDict no root", "[Mutable]") {
    @autoreleasepool {
    NSMutableDictionary* dict;
    @autoreleasepool {
        auto data = encode(@{@"greeting": @"hi",
                             @"array":    @[@"boo", @NO],
                             @"dict":     @{@"melt": @32, @"boil": @212}});
        dict = [FleeceDocument objectFromFleeceSlice: data
                                   mutableContainers: YES];
    }
    NSLog(@"FleeceDict = %@", dict);
    CHECK(!((FleeceDict*)dict).isMutated);
    CHECK(([sortedKeys(dict) isEqual: @[@"array", @"dict", @"greeting"]]));
    CHECK([dict[@"greeting"] isEqual: @"hi"]);
    CHECK(dict[@"x"] == nil);
    verifyDictIterator(dict);

    NSMutableDictionary* nested = dict[@"dict"];
    CHECK(([sortedKeys(nested) isEqual: @[@"boil", @"melt"]]));
    CHECK(([nested isEqual: @{@"melt": @32, @"boil": @212}]));
    CHECK([nested[@"melt"] isEqual: @32]);
    CHECK([nested[@"boil"] isEqual: @212]);
    CHECK(nested[@"freeze"] == nil);
    CHECK(([nested isEqual: @{@"melt": @32, @"boil": @212}]));
    verifyDictIterator(nested);
    CHECK(!((FleeceDict*)nested).isMutated);
    CHECK(!((FleeceDict*)dict).isMutated);

    nested[@"freeze"] = @[@32, @"Fahrenheit"];
    CHECK(((FleeceArray*)nested).isMutated);
    CHECK(((FleeceDict*)dict).isMutated);
    [nested removeObjectForKey: @"melt"];
    CHECK(([nested isEqual: @{@"freeze": @[@32, @"Fahrenheit"], @"boil": @212}]));
    verifyDictIterator(nested);
    verifyDictIterator(dict);

    CHECK(fleece2JSON(encode(dict)) == "{array:[\"boo\",false],dict:{boil:212,freeze:[32,\"Fahrenheit\"]},greeting:\"hi\"}");
    }
#if DEBUG
    CHECK(MContext::gInstanceCount == 0);
#endif
}


TEST_CASE("Adding mutable collections", "[Mutable]") {
    @autoreleasepool {
    auto data = encode(@{@"array":    @[@"boo", @NO],
                         @"dict":     @{@"boil": @212, @"melt": @32, },
                         @"greeting": @"hi"});
    MRoot<id> root(data);
    CHECK(!root.isMutated());
    NSMutableDictionary* dict = root.asNative();

    id array = dict[@"array"];
    dict[@"new"] = array;
    [array addObject: @YES];
    CHECK(fleece2JSON(encode(root)) == "{array:[\"boo\",false,true],dict:{boil:212,melt:32},greeting:\"hi\",new:[\"boo\",false,true]}");
    }
#if DEBUG
    CHECK(MContext::gInstanceCount == 0);
#endif
}
