//
// Fleece+CoreFoundation.mm
//
// Copyright 2016-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#import "Fleece+ImplGlue.hh"
#import "fleece/Fleece+CoreFoundation.h"
#import <Foundation/Foundation.h>

using namespace fleece;
using namespace fleece::impl;

NSString* const FLErrorDomain = @"Fleece";


NSMapTable* FLCreateSharedStringsTable(void) FLAPI {
    return Value::createSharedStringsTable();
}


id FLValue_GetNSObject(FLValue value, NSMapTable* sharedStrings) FLAPI {
    try {
        if (value)
            return value->toNSObject(sharedStrings);
    } catchError(nullptr)
    return nil;
}


FLValue FLDict_GetWithNSString(FLDict dict, NSString* key) FLAPI {
    try {
        if (dict)
            return dict->get(key);
    } catchError(nullptr)
    return nullptr;
}


NSString* FLDictIterator_GetKeyAsNSString(const FLDictIterator *i,
                                          __unsafe_unretained NSMapTable *sharedStrings) FLAPI
{
    return ((Dict::iterator*)i)->keyToNSString(sharedStrings);
}


NSData* FLEncoder_FinishWithNSData(FLEncoder enc, NSError** outError) FLAPI {
    FLError code;
    FLSliceResult result = FLEncoder_Finish(enc, &code);
    if (result.buf)
        return alloc_slice(result).uncopiedNSData();
    if (outError)
        *outError = [NSError errorWithDomain: FLErrorDomain code: code userInfo: nullptr];
    return nil;
}



bool FLEncoder_WriteCFObject(FLEncoder encoder, CFTypeRef obj) FLAPI {
    return FLEncoder_WriteNSObject(encoder, (__bridge id)obj);
}


CFTypeRef FLValue_CopyCFObject(FLValue value) FLAPI {
    id obj = FLValue_GetNSObject(value, nullptr);
    return obj ? CFBridgingRetain(obj) : nullptr;
}


FLValue FLDict_GetWithCFString(FLDict dict, CFStringRef key) FLAPI {
    return FLDict_GetWithNSString(dict, (__bridge NSString*)key);
}
