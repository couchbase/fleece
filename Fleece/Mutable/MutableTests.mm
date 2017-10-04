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
    MValue<id> val(@"hi");
    REQUIRE([val.asNative(nullptr, false) isEqual: @"hi"]);
    REQUIRE(val.value() == nullptr);
}




TEST_CASE("MDict", "[Mutable]") {
    auto data = encode(@{@"greeting": @"hi",
                         @"array":    @[@"boo", @NO],
                         @"dict":     @{@"melt": @32, @"boil": @212}});
    MRoot<id> root(data);
    CHECK(!root.isMutated());
    NSMutableDictionary* dict = root.asNative();
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
}


TEST_CASE("MArray", "[Mutable]") {
    auto data = encode(@[@"hi", @[@"boo", @NO], @42]);
    MRoot<id> root(data);
    CHECK(!root.isMutated());
    NSMutableArray* array = root.asNative();
    NSLog(@"Check item 0");
    CHECK([array[0] isEqual: @"hi"]);
    NSLog(@"Check item 1");
    CHECK([array[2] isEqual: @42]);
    NSLog(@"Check item 2");
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
