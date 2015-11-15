//
//  Encoding+ObjC.mm
//  Fleece
//
//  Created by Jens Alfke on 1/29/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Value.hh"


namespace fleece {


    NSMapTable* value::createSharedStringsTable() {
        return [[NSMapTable alloc] initWithKeyOptions: NSPointerFunctionsIntegerPersonality |
                                                       NSPointerFunctionsOpaqueMemory
                                         valueOptions: NSPointerFunctionsStrongMemory
                                             capacity: 10];
    }


    id value::asNSObject(__unsafe_unretained NSArray* externStrings) const {
        return asNSObject(createSharedStringsTable(), externStrings);
    }


    id value::asNSObject(__unsafe_unretained NSMapTable *sharedStrings,
                         __unsafe_unretained NSArray* externStrings) const
    {
        switch (type()) {
            case kNull:
                return [NSNull null];
            case kBoolean:
                return @(asBool());
            case kInteger: {
                int64_t i = asInt();
                return isUnsigned() ? @((uint64_t)i) : @(i);
            }
            case kFloat:
                return @(asDouble());
            case kString: {
                NSString* str = (NSString*)asString();
                if (!str)
                    throw "Invalid UTF-8 in string";
                return str;
            }
            case kData:
                return asString().copiedNSData();
            case kArray: {
                const array* a = asArray();
                NSMutableArray* result = [NSMutableArray arrayWithCapacity: a->count()];
                for (array::iterator iter(a); iter; ++iter) {
                    [result addObject: iter->asNSObject(sharedStrings, externStrings)];
                }
                return result;
            }
            case kDict: {
                const dict* d = asDict();
                size_t count = d->count();
                NSMutableDictionary* result = [NSMutableDictionary dictionaryWithCapacity: count];
                for (dict::iterator iter(d); iter; ++iter) {
                    NSString* key = iter.key()->asNSObject(sharedStrings, externStrings);
                    result[key] = iter.value()->asNSObject(sharedStrings, externStrings);
                }
                return result;
            }
            default:
                throw "illegal typecode";
        }
    }


}
