//
//  Encoding+ObjC.mm
//  Fleece
//
//  Created by Jens Alfke on 1/29/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
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
#include "FleeceException.hh"


namespace fleece {

    NSMapTable* Value::createSharedStringsTable() {
        return [[NSMapTable alloc] initWithKeyOptions: NSPointerFunctionsOpaquePersonality |
                                                       NSPointerFunctionsOpaqueMemory
                                         valueOptions: NSPointerFunctionsStrongMemory
                                             capacity: 8];
    }


    id Value::toNSObject(__unsafe_unretained NSMapTable *sharedStrings) const
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
                        return CFBridgingRelease(CFNumberCreate(NULL, kCFNumberLongLongType,  &i));
                } else if (isDouble()) {
                    double d = asDouble();
                    return CFBridgingRelease(CFNumberCreate(NULL, kCFNumberDoubleType,  &d));
                } else {
                    float f = asFloat();
                    return CFBridgingRelease(CFNumberCreate(NULL, kCFNumberFloatType,  &f));
                }
            case kString: {
                slice strSlice = asString();
                bool shareable = (sharedStrings != nil
                               && strSlice.size >= internal::kMinSharedStringSize
                               && strSlice.size <= internal::kMaxSharedStringSize);
                if (shareable) {
#if TARGET_OS_IPHONE
                    NSString* str = [sharedStrings objectForKey: (__bridge id)this];
#else
                    NSString* str = (__bridge NSString*)NSMapGet(sharedStrings, this);
#endif
                    if (str)
                        return str;
                }
                NSString* str = (NSString*)strSlice;
                if (!str)
                    throw FleeceException("Invalid UTF-8 in string");
                if (shareable) {
#if TARGET_OS_IPHONE
                    [sharedStrings setObject: str forKey: (__bridge id)this];
#else
                    NSMapInsert(sharedStrings, this, (__bridge void*)str);
#endif
                }
                return str;
            }
            case kData:
                return asString().copiedNSData();
            case kArray: {
                auto iter = asArray()->begin();
                auto result = [[NSMutableArray alloc] initWithCapacity: iter.count()];
                for (; iter; ++iter) {
                    [result addObject: iter->toNSObject(sharedStrings)];
                }
                return result;
            }
            case kDict: {
                Dict::iterator iter(asDict());
                auto result = [[NSMutableDictionary alloc] initWithCapacity: iter.count()];
                for (; iter; ++iter) {
                    NSString* key = iter.key()->toNSObject(sharedStrings);
                    result[key] = iter.value()->toNSObject(sharedStrings);
                }
                return result;
            }
            default:
                throw FleeceException("illegal typecode");
        }
    }


}
