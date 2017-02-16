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


NSMapTable* FLCreateSharedStringsTable(void) {
    return Value::createSharedStringsTable();
}


bool FLEncoder_WriteNSObject(FLEncoder encoder, id obj) {
    ENCODER_TRY(encoder, writeObjC(obj));
}


id FLValue_GetNSObject(FLValue value, FLSharedKeys sharedKeys, NSMapTable* sharedStrings) {
    try {
        if (value)
            return value->toNSObject(sharedStrings, sharedKeys);
    } catchError(nullptr)
    return nullptr;
}


FLValue FLDict_GetWithNSString(FLDict dict, NSString* key) {
    try {
        if (dict)
            return dict->get(key);
    } catchError(nullptr)
    return nullptr;
}


NSString* FLDictIterator_GetKeyAsNSString(FLDictIterator *i,
                                          __unsafe_unretained NSMapTable *sharedStrings,
                                          FLSharedKeys sk)
{
    return ((Dict::iterator*)i)->keyToNSString(sharedStrings, (const SharedKeys*)sk);
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


CFTypeRef FLValue_CopyCFObject(FLValue value, FLSharedKeys sharedKeys) {
    id obj = FLValue_GetNSObject(value, sharedKeys, nullptr);
    return obj ? CFBridgingRetain(obj) : nullptr;
}


FLValue FLDict_GetWithCFString(FLDict dict, CFStringRef key) {
    return FLDict_GetWithNSString(dict, (__bridge NSString*)key);
}
