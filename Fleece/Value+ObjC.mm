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
        switch (_typeCode) {
            case kNullCode:
                return [NSNull null];
            case kFalseCode:
                return (__bridge id)kCFBooleanFalse;
            case kTrueCode:
                return (__bridge id)kCFBooleanTrue;
            case kInt1Code...kInt8Code:
                return @(asInt());
            case kUInt64Code:
                return @(asUnsigned());
            case kFloat32Code:
            case kFloat64Code:
                return @(asDouble());
            case kRawNumberCode:
                return [NSDecimalNumber decimalNumberWithString: (NSString*)asString()];
            case kDateCode:
                return [NSDate dateWithTimeIntervalSince1970: asDate()];
            case kStringCode: {
                NSString* str = (NSString*)asString();
                if (!str)
                    throw "Invalid UTF-8 in string";
                return str;
            }
            case kSharedStringRefCode: {
                NSString* str = [sharedStrings objectForKey: (__bridge id)(void*)stringToken()];
                if (str)
                    return str;
                // If not already cached in sharedStrings, fall through to add it...
            }
            case kSharedStringCode: {
                NSString* str = (NSString*)asString();
                if (!str)
                    throw "Invalid UTF-8 in string";
                [sharedStrings setObject: str forKey: (__bridge id)(void*)stringToken()];
                return str;
            }
            case kExternStringRefCode: {
                if (!externStrings)
                    throw "unexpected extern string";
                uint64_t stringIndex = stringToken();
                if (stringIndex > UINT32_MAX)
                    throw "invalid extern string index";
                return externStrings[(uint32_t)stringIndex - 1];
            }
            case kDataCode:
                return asString().copiedNSData();
            case kArrayCode: {
                const array* a = asArray();
                NSMutableArray* result = [NSMutableArray arrayWithCapacity: a->count()];
                for (array::iterator iter(a); iter; ++iter) {
                    [result addObject: iter->asNSObject(sharedStrings, externStrings)];
                }
                return result;
            }
            case kDictCode: {
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
