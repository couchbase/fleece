//
// Fleece+CoreFoundation.mm
//
// Copyright (c) 2016 Couchbase, Inc All rights reserved.
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

#import "Fleece_C_impl.hh"
#import "Fleece+CoreFoundation.h"
#import <Foundation/Foundation.h>


NSString* const FLErrorDomain = @"Fleece";


NSMapTable* FLCreateSharedStringsTable(void) {
    return Value::createSharedStringsTable();
}


bool FLEncoder_WriteNSObject(FLEncoder encoder, id obj) {
    try {
        if (!encoder->hasError()) {
            throwIf(!obj, InvalidData, "Can't encode nil");
            [obj fl_encodeToFLEncoder: encoder];
        }
        return true;
    } catch (const std::exception &x) {
        encoder->recordException(x);
    }
    return false;
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


NSString* FLDictIterator_GetKeyAsNSString(const FLDictIterator *i,
                                          __unsafe_unretained NSMapTable *sharedStrings)
{
    return ((Dict::iterator*)i)->keyToNSString(sharedStrings);
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
