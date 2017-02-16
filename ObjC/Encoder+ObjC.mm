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


@interface NSObject (Fleece)
- (void) fl_encodeTo: (fleece::Encoder*)enc;
@end


namespace fleece {

    void Encoder::writeObjC(__unsafe_unretained id obj) {
        throwIf(!obj, InvalidData, "Can't encode nil");
        [obj fl_encodeTo: this];
    }

}


using namespace fleece;


@implementation NSNull (CBJSONEncoder)
- (void) fl_encodeTo: (fleece::Encoder*)enc {
    enc->writeNull();
}
@end

@implementation NSNumber (CBJSONEncoder)
- (void) fl_encodeTo: (Encoder*)enc {
    switch (self.objCType[0]) {
        case 'b':
            enc->writeBool(self.boolValue);
            break;
        case 'c':
            // The only way to tell whether an NSNumber with 'char' type is a boolean is to
            // compare it against the singleton kCFBoolean objects:
            if (self == (id)kCFBooleanTrue)
                enc->writeBool(true);
            else if (self == (id)kCFBooleanFalse)
                enc->writeBool(false);
            else
                enc->writeInt(self.charValue);
            break;
        case 'f':
            enc->writeFloat(self.floatValue);
            break;
        case 'd':
            enc->writeDouble(self.doubleValue);
            break;
        case 'Q':
            enc->writeUInt(self.unsignedLongLongValue);
            break;
        default:
            enc->writeInt(self.longLongValue);
            break;
    }
}
@end

@implementation NSString (CBJSONEncoder)
- (void) fl_encodeTo: (Encoder*)enc {
    nsstring_slice s(self);
    enc->writeString(s);
}
@end

@implementation NSData (CBJSONEncoder)
- (void) fl_encodeTo: (Encoder*)enc {
    enc->writeData(slice(self));
}
@end

@implementation NSArray (CBJSONEncoder)
- (void) fl_encodeTo: (Encoder*)enc {
    enc->beginArray((uint32_t)self.count);
    for (NSString* item in self) {
        [item fl_encodeTo: enc];
    }
    enc->endArray();
}
@end

@implementation NSDictionary (CBJSONEncoder)
- (void) fl_encodeTo: (Encoder*)enc {
    enc->beginDictionary((uint32_t)self.count);
    [self enumerateKeysAndObjectsUsingBlock:^(__unsafe_unretained id key,
                                              __unsafe_unretained id value, BOOL *stop) {
        nsstring_slice slice(key);
        enc->writeKey(slice);
        [value fl_encodeTo: enc];
    }];
    enc->endDictionary();
}
@end
