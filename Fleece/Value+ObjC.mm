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


namespace fleece {

    NSMapTable* value::createSharedStringsTable() {
        return [[NSMapTable alloc] initWithKeyOptions: NSPointerFunctionsOpaquePersonality |
                                                       NSPointerFunctionsOpaqueMemory
                                         valueOptions: NSPointerFunctionsStrongMemory
                                             capacity: 1000];
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
                bool shareable = (strSlice.size >= internal::kMinSharedStringSize
                               && strSlice.size <= internal::kMaxSharedStringSize);
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
                array::iterator iter(asArray());
                auto result = [[NSMutableArray alloc] initWithCapacity: iter.count()];
                for (; iter; ++iter) {
                    [result addObject: iter->asNSObject(sharedStrings)];
                }
                return result;
            }
            case kDict: {
                dict::iterator iter(asDict());
                auto result = [[NSMutableDictionary alloc] initWithCapacity: iter.count()];
                for (; iter; ++iter) {
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
