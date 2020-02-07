//
// Encoder+ObjC.mm
//
// Copyright (c) 2015 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#import <Foundation/Foundation.h>
#import "Encoder.hh"
#import "Fleece+ImplGlue.hh"
#import "FleeceException.hh"

using namespace fleece::impl;


@interface NSObject (FleeceInternal)
- (void) fl_encodeTo: (Encoder*)enc;
@end


namespace fleece {

    void Encoder::writeObjC(__unsafe_unretained id obj) {
        throwIf(!obj, InvalidData, "Can't encode nil");
        FLEncoderImpl enc(this);
        [obj fl_encodeToFLEncoder: &enc];
    }

}


using namespace fleece;


bool FLEncoder_WriteNSObject(FLEncoder encoder, id obj) FLAPI {
    try {
        if (!encoder->hasError()) {
            throwIf(!obj, InvalidData, "Can't encode nil");
            [obj fl_encodeToFLEncoder: encoder];
        }
        return true;
    } catch (const std::exception &x) {
        encoder->recordException(x);
    } catch (NSException* e) {
        encoder->recordException(FleeceException(EncodeError, 0, e.reason.UTF8String));
    }
    return false;
}


@implementation NSObject (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    // Fall back to the internal fl_encodeTo:, which takes a raw C++ Encoder*.
    [self fl_encodeTo: ((FLEncoderImpl*)enc)->fleeceEncoder.get()];
}
@end


@implementation NSObject (FleeceInternal)
- (void) fl_encodeTo: (Encoder*)enc {
    // Default implementation -- object doesn't implement Fleece encoding at all.
    NSString* msg = [NSString stringWithFormat: @"Objects of class %@ cannot be encoded",
                     [self class]];
    FleeceException::_throw(EncodeError, "%s", msg.UTF8String);
}
@end


@implementation NSNull (Fleece)
- (void) fl_encodeTo: (Encoder*)enc {
    enc->writeNull();
}
@end

@implementation NSNumber (Fleece)
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

@implementation NSString (Fleece)
- (void) fl_encodeTo: (Encoder*)enc {
    nsstring_slice s(self);
    enc->writeString(s);
}
@end

@implementation NSData (Fleece)
- (void) fl_encodeTo: (Encoder*)enc {
    enc->writeData(slice(self));
}
@end

@implementation NSArray (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    FLEncoder_BeginArray(enc, (uint32_t)self.count);
    for (NSString* item in self) {
        [item fl_encodeToFLEncoder: enc];
    }
    FLEncoder_EndArray(enc);
}

- (void) fl_encodeTo: (Encoder*)enc {
    enc->beginArray((uint32_t)self.count);
    for (NSString* item in self) {
        [item fl_encodeTo: enc];
    }
    enc->endArray();
}
@end

@implementation NSDictionary (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    FLEncoder_BeginDict(enc, (uint32_t)self.count);
    [self enumerateKeysAndObjectsUsingBlock:^(__unsafe_unretained id key,
                                              __unsafe_unretained id value, BOOL *stop) {
        nsstring_slice slice(key);
        FLEncoder_WriteKey(enc, slice);
        [value fl_encodeToFLEncoder: enc];
    }];
    FLEncoder_EndDict(enc);
}

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
