//
//  Fleece+CoreFoundation.mm
//  Fleece
//
//  Created by Jens Alfke on 9/19/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#import "Fleece_C_impl.hh"
#import "Fleece+CoreFoundation.h"
#import <Foundation/Foundation.h>


NSString* const FLErrorDomain = @"Fleece";


bool FLEncoder_WriteNSObject(FLEncoder encoder, id obj) {
    try {
        encoder->write(obj);
        return true;
    } catchError(nullptr)
    return false;
}


id FLValue_GetNSObject(FLValue value) {
    try {
        return value->toNSObject();
    } catchError(nullptr)
    return nullptr;
}


FLValue FLDict_GetWithNSString(FLDict dict, NSString* key) {
    try {
        return dict->get(key);
    } catchError(nullptr)
    return nullptr;
}


NSData* FLEncoder_FinishWithNSData(FLEncoder enc, NSError** outError) {
    FLError code;
    FLSliceResult result = FLEncoder_Finish(enc, &code);
    if (result.buf)
        return [NSData dataWithBytesNoCopy: result.buf length: result.size freeWhenDone: YES];
    if (outError)
        *outError = [NSError errorWithDomain: FLErrorDomain code: code userInfo: nullptr];
    return nil;
}



bool FLEncoder_WriteCFObject(FLEncoder encoder, CFTypeRef obj) {
    return FLEncoder_WriteNSObject(encoder, (__bridge id)obj);
}


CFTypeRef FLValue_CopyCFObject(FLValue value) {
    id obj = FLValue_GetNSObject(value);
    return obj ? CFBridgingRetain(obj) : nullptr;
}


FLValue FLDict_GetWithCFString(FLDict dict, CFStringRef key) {
    return FLDict_GetWithNSString(dict, (__bridge NSString*)key);
}
