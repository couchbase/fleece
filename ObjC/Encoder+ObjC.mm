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
- (void) fl_encodeToFLEncoder: (FLEncoder)enc;
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
    [self fl_encodeToFLEncoder: enc];
}
@end


@implementation NSObject (FleeceInternal)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    // Default implementation -- object doesn't implement Fleece encoding at all.
    NSString* msg = [NSString stringWithFormat: @"Objects of class %@ cannot be encoded",
                     [self class]];
    FleeceException::_throw(EncodeError, "%s", msg.UTF8String);
}

@end


@implementation NSNull (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    ENCODER_DO(enc, writeNull());
}

@end

@implementation NSNumber (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    switch (self.objCType[0]) {
        case 'b':
            ENCODER_DO(enc, writeBool(self.boolValue));
            break;
        case 'c':
            // The only way to tell whether an NSNumber with 'char' type is a boolean is to
            // compare it against the singleton kCFBoolean objects:
            if (self == (id)kCFBooleanTrue)
                ENCODER_DO(enc, writeBool(true));
            else if (self == (id)kCFBooleanFalse)
                ENCODER_DO(enc, writeBool(false));
            else
                ENCODER_DO(enc, writeInt(self.charValue));
            break;
        case 'f':
            ENCODER_DO(enc, writeFloat(self.floatValue));
            break;
        case 'd':
            ENCODER_DO(enc, writeDouble(self.doubleValue));
            break;
        case 'Q':
            ENCODER_DO(enc, writeUInt(self.unsignedLongLongValue));
            break;
        default:
            ENCODER_DO(enc, writeInt(self.longLongValue));
            break;
    }
}

@end

@implementation NSString (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    nsstring_slice s(self);
    ENCODER_DO(enc, writeString(s));
}

@end

@implementation NSData (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    ENCODER_DO(enc, writeData(slice(self)));
}

@end

@implementation NSArray (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    ENCODER_DO(enc, beginArray((uint32_t)self.count));
    for (NSString* item in self) {
        [item fl_encodeToFLEncoder: enc];
    }
    ENCODER_DO(enc, endArray());
}

@end

@implementation NSDictionary (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    ENCODER_DO(enc, beginDictionary((uint32_t)self.count));
    [self enumerateKeysAndObjectsUsingBlock:^(__unsafe_unretained id key,
                                              __unsafe_unretained id value, BOOL *stop) {
        nsstring_slice slice(key);
        ENCODER_DO(enc, writeKey(slice));
        [value fl_encodeToFLEncoder: enc];
    }];
    ENCODER_DO(enc, endDictionary());
}

@end
