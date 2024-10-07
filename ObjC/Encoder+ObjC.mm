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
#import "fleece/Fleece+CoreFoundation.h"

using namespace fleece::impl;


namespace fleece {
    void Encoder::writeObjC(__unsafe_unretained id obj) {
        throwIf(!obj, InvalidData, "Can't encode nil");
        FLEncoderImpl enc(this);
        [obj fl_encodeToFLEncoder: &enc];
    }

    void Encoder::writeCF(CFTypeRef obj) {
        writeObjC((__bridge id)obj);
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


FLValue FLValue_FromNSObject(__unsafe_unretained id obj) {
    return [obj fl_convertToFleece];
}

FLValue FLValue_FromCFValue(CFTypeRef value) {
    return [(__bridge id)value fl_convertToFleece];
}

void FLSlot_SetCFValue(FLSlot slot, CFTypeRef value) {
    return [(__bridge id)value fl_storeInSlot: slot];
}



@implementation NSObject (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc __attribute__((noreturn)) {
    // Default implementation -- object doesn't implement Fleece encoding at all.
    NSString* msg = [NSString stringWithFormat: @"Objects of class %@ cannot be encoded",
                     [self class]];
    FleeceException::_throw(EncodeError, "%s", msg.UTF8String);
}

- (void) fl_storeInSlot: (FLSlot)slot {
    // By default, convert self to a standalone Fleece value and store that FLValue in the slot.
    FLValue val = self.fl_convertToFleece;
    FLSlot_SetValue(slot, val);
    FLValue_Release(val);
}

- (FLValue) fl_convertToFleece {
    // Default implementation -- object doesn't implement Fleece conversion at all.
    // Only NSArray and NSDictionary override this, since they have standalone Fleece equivalents.
    NSString* msg = [NSString stringWithFormat: @"Objects of class %@ cannot be converted to Fleece",
                     [self class]];
    FleeceException::_throw(InvalidData, "%s", msg.UTF8String);
}

@end


@implementation NSNull (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    FLEncoder_WriteNull(enc);
}

- (void) fl_storeInSlot: (FLSlot)slot {
    FLSlot_SetNull(slot);
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

- (void) fl_storeInSlot: (FLSlot)slot {
    switch (self.objCType[0]) {
        case 'b':
            FLSlot_SetBool(slot, self.boolValue);
            break;
        case 'c':
            // The only way to tell whether an NSNumber with 'char' type is a boolean is to
            // compare it against the singleton kCFBoolean objects:
            if (self == (id)kCFBooleanTrue)
                FLSlot_SetBool(slot, true);
            else if (self == (id)kCFBooleanFalse)
                FLSlot_SetBool(slot, false);
            else
                FLSlot_SetInt(slot, self.charValue);
            break;
        case 'f':
            FLSlot_SetFloat(slot, self.floatValue);
            break;
        case 'd':
            FLSlot_SetDouble(slot, self.doubleValue);
            break;
        case 'Q':
            FLSlot_SetUInt(slot, self.unsignedLongLongValue);
            break;
        default:
            FLSlot_SetInt(slot, self.longLongValue);
            break;
    }
}

@end

@implementation NSString (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    nsstring_slice s(self);
    FLEncoder_WriteString(enc, s);
}

- (void) fl_storeInSlot: (FLSlot)slot {
    nsstring_slice s(self);
    FLSlot_SetString(slot, s);
}

@end

@implementation NSData (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    FLEncoder_WriteData(enc, slice(self));
}

- (void) fl_storeInSlot: (FLSlot)slot {
    FLSlot_SetData(slot, slice(self));
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

- (FLValue) fl_convertToFleece {
    FLMutableArray array = FLMutableArray_New();
    FLMutableArray_Resize(array, uint32_t(self.count));
    uint32_t i = 0;
    for (id val in self)
        [val fl_storeInSlot: FLMutableArray_Set(array, i++)];
    return FLValue(array);
}

@end

@implementation NSDictionary (Fleece)
- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    FLEncoder_BeginDict(enc, (uint32_t)self.count);
    [self enumerateKeysAndObjectsUsingBlock:^(__unsafe_unretained id key,
                                              __unsafe_unretained id value, BOOL *stop) {
        nsstring_slice flKey(key);
        FLEncoder_WriteKey(enc, flKey);
        [value fl_encodeToFLEncoder: enc];
    }];
    FLEncoder_EndDict(enc);
}

- (FLValue) fl_convertToFleece {
    FLMutableDict dict = FLMutableDict_New();
    [self enumerateKeysAndObjectsUsingBlock:^(__unsafe_unretained id key,
                                              __unsafe_unretained id value, BOOL *stop) {
        nsstring_slice flKey(key);
        [value fl_storeInSlot: FLMutableDict_Set(dict, flKey)];
    }];
    return FLValue(dict);
}

@end
