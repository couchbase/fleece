//
//  Value+ObjC.mm
//  Fleece
//
//  Created by Jens Alfke on 1/29/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#import <Foundation/Foundation.h>
#import "Value.hh"
#include "Array.hh"
#include "SharedKeys.hh"
#include "FleeceException.hh"


// NSMapXXX C API isn't available in iOS
#if TARGET_OS_IPHONE
#define MapGet(STRINGS, KEY)           [(STRINGS) objectForKey: (__bridge id)(KEY)]
#define MapInsert(STRINGS, KEY, VALUE) [(STRINGS) setObject: (VALUE) \
                                                     forKey: (__bridge id)(KEY)]
#else
#define MapGet(STRINGS, KEY)           (__bridge NSString*)NSMapGet((STRINGS), (KEY))
#define MapInsert(STRINGS, KEY, VALUE) NSMapInsert((STRINGS), (KEY), (__bridge void*)(VALUE))
#endif


namespace fleece {

    // Creates an NSMapTable that maps opaque pointers to Obj-C objects (NSStrings).
    NSMapTable* Value::createSharedStringsTable() noexcept {
        return [[NSMapTable alloc] initWithKeyOptions: NSPointerFunctionsOpaquePersonality |
                                                       NSPointerFunctionsOpaqueMemory
                                         valueOptions: NSPointerFunctionsObjectPersonality |
                                                       NSPointerFunctionsStrongMemory
                                             capacity: 8];
    }


    static NSString* convertString(slice strSlice) {
        auto str = (NSString*)strSlice;
        throwIf(!str, InvalidData, "Invalid UTF-8 in string");
        return str;
    }


    id Value::toNSObject(__unsafe_unretained NSMapTable *sharedStrings, const SharedKeys *sk) const
    {
        switch (type()) {
            case kNull:
                return [NSNull null];
            case kBoolean:
                return @(asBool());
            case kNumber:
                // It's faster to use CFNumber, than NSNumber or @(n)
                if (isInteger()) {
                    int64_t i = asInt();
                    if (isUnsigned())
                        return @((uint64_t)i);  // CFNumber can't do unsigned long long!
                    else
                        return CFBridgingRelease(CFNumberCreate(nullptr, kCFNumberLongLongType,  &i));
                } else if (isDouble()) {
                    double d = asDouble();
                    return CFBridgingRelease(CFNumberCreate(nullptr, kCFNumberDoubleType,  &d));
                } else {
                    float f = asFloat();
                    return CFBridgingRelease(CFNumberCreate(nullptr, kCFNumberFloatType,  &f));
                }
            case kString: {
                slice strSlice = asString();
                if (sharedStrings != nil
                               && strSlice.size >= internal::kMinSharedStringSize
                               && strSlice.size <= internal::kMaxSharedStringSize) {
                    // Look up an existing shared string for this Value*:
                    NSString* str = MapGet(sharedStrings, this);
                    if (!str) {
                        str = convertString(strSlice);
                        MapInsert(sharedStrings, this, str);
                    }
                    return str;
                } else {
                    return convertString(strSlice);
                }
            }
            case kData:
                return asData().copiedNSData();
            case kArray: {
                auto iter = asArray()->begin();
                auto result = [[NSMutableArray alloc] initWithCapacity: iter.count()];
                for (; iter; ++iter) {
                    [result addObject: iter->toNSObject(sharedStrings, sk)];
                }
                return result;
            }
            case kDict: {
                Dict::iterator iter(asDict());
                auto result = [[NSMutableDictionary alloc] initWithCapacity: iter.count()];
                for (; iter; ++iter) {
                    NSString* key = nil;
                    if (iter.key()->isInteger() && sk) {
                        // Decode int key using SharedKeys:
                        auto encodedKey = (int)iter.key()->asInt();
                        key = (__bridge NSString*)sk->platformStringForKey(encodedKey);
                        if (!key) {
                            slice strSlice = sk->decode(encodedKey);
                            if (strSlice) {
                                key = convertString(strSlice);
                                sk->setPlatformStringForKey(encodedKey,
                                                            CFRetain((__bridge CFStringRef)key));
                                //TODO: Strings need to be CFRelease'd when the SharedKeys is destructed.
                            }
                        }
                    }
                    if (!key)
                        key = iter.key()->toNSObject(sharedStrings, sk);
                    result[key] = iter.value()->toNSObject(sharedStrings, sk);
                }
                return result;
            }
            default:
                FleeceException::_throw(UnknownValue, "illegal typecode in Value; corrupt data?");
        }
    }


}
