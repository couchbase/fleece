//
//  Encoder.mm
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
#import "Encoder.hh"
#include "FleeceException.hh"


namespace fleece {


    void Encoder::write(__unsafe_unretained id obj) {
        if ([obj isKindOfClass: [NSString class]]) {
            nsstring_slice slice(obj);
            writeString(slice);
        } else if ([obj isKindOfClass: [NSNumber class]]) {
            switch ([obj objCType][0]) {
                case 'c':
                    // The only way to tell whether an NSNumber with 'char' type is a boolean is to
                    // compare it against the singleton kCFBoolean objects:
                    if (obj == (id)kCFBooleanTrue)
                        writeBool(true);
                    else if (obj == (id)kCFBooleanFalse)
                        writeBool(false);
                    else
                        writeInt([obj charValue]);
                    break;
                case 'f':
                    writeFloat([obj floatValue]);
                    break;
                case 'd':
                    writeDouble([obj doubleValue]);
                    break;
                case 'Q':
                    writeUInt([obj unsignedLongLongValue]);
                    break;
                default:
                    writeInt([obj longLongValue]);
                    break;
            }
        } else if ([obj isKindOfClass: [NSDictionary class]]) {
            beginDictionary((uint32_t)[obj count]);
            [obj enumerateKeysAndObjectsUsingBlock:^(__unsafe_unretained id key,
                                                     __unsafe_unretained id value, BOOL *stop) {
                if (![key isKindOfClass: [NSString class]])
                    throw FleeceException(InvalidData, "NSDictionary has non-string key");
                nsstring_slice slice(key);
                writeKey(slice);
                write(value);
            }];
            endDictionary();
        } else if ([obj isKindOfClass: [NSArray class]]) {
            beginArray((uint32_t)[obj count]);
            for (NSString* item in obj) {
                write(item);
            }
            endArray();
        } else if ([obj isKindOfClass: [NSData class]]) {
            writeData(slice((NSData*)obj));
        } else if ([obj isKindOfClass: [NSNull class]]) {
            writeNull();
        } else {
            throw FleeceException(InvalidData, "Un-encodable object type");
        }
    }

}
