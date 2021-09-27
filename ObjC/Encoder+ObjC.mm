//
// Encoder+ObjC.mm
//
// Copyright 2015-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#import <Foundation/Foundation.h>
#import "Encoder.hh"
#import "Fleece+ImplGlue.hh"
#import "FleeceException.hh"

using namespace fleece::impl;


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
    // Default implementation -- object doesn't implement Fleece encoding at all.
    NSString* msg = [NSString stringWithFormat: @"Objects of class %@ cannot be encoded",
                     [self class]];
    FleeceException::_throw(EncodeError, "%s", msg.UTF8String);
}

@end


@implementation NSNull (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    FLEncoder_WriteNull(enc);
}

@end

@implementation NSNumber (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    switch (self.objCType[0]) {
        case 'b':
            FLEncoder_WriteBool(enc, self.boolValue);
            break;
        case 'c':
            // The only way to tell whether an NSNumber with 'char' type is a boolean is to
            // compare it against the singleton kCFBoolean objects:
            if (self == (id)kCFBooleanTrue)
                FLEncoder_WriteBool(enc, true);
            else if (self == (id)kCFBooleanFalse)
                FLEncoder_WriteBool(enc, false);
            else
                FLEncoder_WriteInt(enc, self.charValue);
            break;
        case 'f':
            FLEncoder_WriteFloat(enc, self.floatValue);
            break;
        case 'd':
            FLEncoder_WriteDouble(enc, self.doubleValue);
            break;
        case 'Q':
            FLEncoder_WriteUInt(enc, self.unsignedLongLongValue);
            break;
        default:
            FLEncoder_WriteInt(enc, self.longLongValue);
            break;
    }
}

@end

@implementation NSString (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    nsstring_slice s(self);
    FLEncoder_WriteString(enc, s);
}

@end

@implementation NSData (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    FLEncoder_WriteData(enc, slice(self));
}

@end

@implementation NSArray (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    FLEncoder_BeginArray(enc, (uint32_t)self.count);
    for (id item in self) {
        [item fl_encodeToFLEncoder: enc];
    }
    FLEncoder_EndArray(enc);
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

@end
