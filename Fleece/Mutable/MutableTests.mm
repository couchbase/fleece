//
//  MutableTests.m
//  Fleece
//
//  Created by Jens Alfke on 5/30/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include <Foundation/Foundation.h>
#include "FleeceTests.hh"
#include "MutableDict+ObjC.hh"
#include "MRoot.hh"
#include "FleeceDocument.h"


static alloc_slice encode(id obj) {
    Encoder enc;
    enc.writeObjC(obj);
    enc.end();
    return enc.extractOutput();
}


static alloc_slice encode(const MRoot<id> &val) {
    Encoder enc;
    val.encodeTo(enc);
    enc.end();
    return enc.extractOutput();
}


static NSArray* sortedKeys(NSDictionary* dict) {
    return [dict.allKeys sortedArrayUsingSelector: @selector(compare:)];
}


static std::string fleece2JSON(alloc_slice fleece) {
    auto v = Value::fromData(fleece);
    if (!v)
        return "INVALID_FLEECE";
    return v->toJSON<5>().asString();
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

    nested[@"freeze"] = @[@32, @"Fahrenheit"];
    CHECK(root.isMutated());
    [nested removeObjectForKey: @"melt"];
    CHECK(([nested isEqual: @{@"freeze": @[@32, @"Fahrenheit"], @"boil": @212}]));

    CHECK(fleece2JSON(encode(dict)) == "{array:[\"boo\",false],dict:{boil:212,freeze:[32,\"Fahrenheit\"]},greeting:\"hi\"}");
    CHECK(fleece2JSON(encode(root)) == "{array:[\"boo\",false],dict:{boil:212,freeze:[32,\"Fahrenheit\"]},greeting:\"hi\"}");

    alloc_slice newData = root.encode();

    // Delta encoding:
    alloc_slice delta = root.encodeDelta();
    REQUIRE(delta.buf != nullptr);
    CHECK(delta.size == 52); // may change

    alloc_slice combinedData(data);
    combinedData.append(delta);
    const Dict* newDict = Value::fromData(combinedData)->asDict();
    std::cerr << "\nContents:      " << newDict->toJSON().asString() << "\n";
    std::cerr <<   "Old:           " << data.size << " bytes: " << data << "\n\n";
    std::cerr <<   "New:           " << newData.size << " bytes: " << newData << "\n\n";
    std::cerr <<   "Delta:         " << delta.size << " bytes: " << delta << "\n\n";
    Value::dump(combinedData, std::cerr);
    }
    CHECK(internal::Context::gInstanceCount == 0);
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
    CHECK(internal::Context::gInstanceCount == 0);
}


TEST_CASE("MDict no root", "[Mutable]") {
    @autoreleasepool {
    NSMutableDictionary* dict;
    @autoreleasepool {
        auto data = encode(@{@"greeting": @"hi",
                             @"array":    @[@"boo", @NO],
                             @"dict":     @{@"melt": @32, @"boil": @212}});
        dict = [FleeceDocument objectFromFleeceSlice: data
                                          sharedKeys: nullptr
                                   mutableContainers: YES];
    }
    NSLog(@"FleeceDict = %@", dict);
    //CHECK(!((FleeceDict*)dict).isMutated);
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
    //CHECK(!((FleeceDict*)dict).isMutated);

    nested[@"freeze"] = @[@32, @"Fahrenheit"];
    //CHECK(((FleeceDict*)dict).isMutated);
    [nested removeObjectForKey: @"melt"];
    CHECK(([nested isEqual: @{@"freeze": @[@32, @"Fahrenheit"], @"boil": @212}]));

    CHECK(fleece2JSON(encode(dict)) == "{array:[\"boo\",false],dict:{boil:212,freeze:[32,\"Fahrenheit\"]},greeting:\"hi\"}");
    }
    CHECK(internal::Context::gInstanceCount == 0);
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
    CHECK(internal::Context::gInstanceCount == 0);
}
