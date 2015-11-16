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
        return [[NSMapTable alloc] initWithKeyOptions: NSPointerFunctionsOpaquePersonality |
                                                       NSPointerFunctionsOpaqueMemory
                                         valueOptions: NSPointerFunctionsStrongMemory
                                             capacity: 10];
    }


    id value::asNSObject() const {
        return asNSObject(createSharedStringsTable());
    }


    id value::asNSObject(__unsafe_unretained NSMapTable *sharedStrings) const
    {
        switch (type()) {
            case kNull:
                return [NSNull null];
            case kBoolean:
                return @(asBool());
            case kNumber:
                if (isInteger()) {
                    int64_t i = asInt();
                    return isUnsigned() ? @((uint64_t)i) : @(i);
                } else {
                    return @(asDouble());
                }
            case kString: {
                slice strSlice = asString();
                bool shareable = (strSlice.size > 1 && strSlice.size < 100);
                if (shareable) {
                    NSString* str = (__bridge NSString*)NSMapGet(sharedStrings, this);
                    if (str)
                        return str;
                }
                NSString* str = (NSString*)strSlice;
                if (!str)
                    throw "Invalid UTF-8 in string";
                if (shareable)
                    NSMapInsert(sharedStrings, this, (__bridge void*)str);
                return str;
            }
            case kData:
                return asString().copiedNSData();
            case kArray: {
                const array* a = asArray();
                NSMutableArray* result = [NSMutableArray arrayWithCapacity: a->count()];
                for (array::iterator iter(a); iter; ++iter) {
                    [result addObject: iter->asNSObject(sharedStrings)];
                }
                return result;
            }
            case kDict: {
                const dict* d = asDict();
                size_t count = d->count();
                NSMutableDictionary* result = [NSMutableDictionary dictionaryWithCapacity: count];
                for (dict::iterator iter(d); iter; ++iter) {
                    NSString* key = iter.key()->asNSObject(sharedStrings);
                    result[key] = iter.value()->asNSObject(sharedStrings);
                }
                return result;
            }
            default:
                throw "illegal typecode";
        }
    }


}
