//
// Fleece+CoreFoundation.h
//
// Copyright 2016-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include <CoreFoundation/CFBase.h>
#include "Fleece.h"

#ifdef __cplusplus
extern "C" {
#endif

    /** \defgroup CF    Fleece CoreFoundation and Objective-C Helpers
        @{ */

    /** Writes a Core Foundation (or Objective-C) object to an Encoder.
        Supports all the JSON types, as well as CFData. */
    bool FLEncoder_WriteCFObject(FLEncoder, CFTypeRef) FLAPI;


    /** Returns a Value as a corresponding CoreFoundation object.
        Caller must CFRelease the result. */
    CFTypeRef FLValue_CopyCFObject(FLValue) FLAPI;


    /** Same as FLDictGet, but takes the key as a CFStringRef. */
    FLValue FLDict_GetWithCFString(FLDict, CFStringRef) FLAPI;


#ifdef __OBJC__
#import <Foundation/NSMapTable.h>
    
    // Equivalents of the above functions that take & return Objective-C object types:
    
    /** Writes a Core Foundation (or Objective-C) object to an Encoder.
        Supports all the JSON types, as well as CFData. */
    bool FLEncoder_WriteNSObject(FLEncoder, id) FLAPI;


    /** Creates an NSMapTable configured for storing shared NSStrings for Fleece decoding. */
    NSMapTable* FLCreateSharedStringsTable(void) FLAPI;

    
    /** Returns a Value as a corresponding (autoreleased) Foundation object. */
    id FLValue_GetNSObject(FLValue, NSMapTable *sharedStrings) FLAPI;


    /** Same as FLDictGet, but takes the key as an NSString. */
    FLValue FLDict_GetWithNSString(FLDict, NSString*) FLAPI;


    /** Returns an FLDictIterator's current key as an NSString. */
    NSString* FLDictIterator_GetKeyAsNSString(const FLDictIterator *i,
                                              NSMapTable *sharedStrings) FLAPI;

    /** Same as FLEncoder_Finish, but returns result as NSData or error as NSError. */
    NSData* FLEncoder_FinishWithNSData(FLEncoder, NSError**) FLAPI;


    /** NSError domain string for Fleece errors */
    extern NSString* const FLErrorDomain;


    @interface NSObject (Fleece)
    /** This method is called on objects being encoded by
        FLEncoder_WriteNSObject (even recursively) if the encoder doesn't know how to encode
        them. You can implement this method in your classes. In it, call the encoder to write
        a single object (which may of course be an array or dictionary.) */
    - (void) fl_encodeToFLEncoder: (FLEncoder)enc;
    @end


#endif

/** @} */

#ifdef __cplusplus
}
#endif
