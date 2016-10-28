//
//  Fleece+CoreFoundation.h
//  Fleece
//
//  Created by Jens Alfke on 9/19/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include <CoreFoundation/CFBase.h>
#include "Fleece.h"

#ifdef __cplusplus
extern "C" {
#endif


    /** Writes a Core Foundation (or Objective-C) object to an Encoder.
        Supports all the JSON types, as well as CFData. */
    bool FLEncoder_WriteCFObject(FLEncoder, CFTypeRef);


    /** Returns a Value as a corresponding CoreFoundation object.
        Caller must CFRelease the result. */
    CFTypeRef FLValue_CopyCFObject(FLValue, FLSharedKeys);


    /** Same as FLDictGet, but takes the key as a CFStringRef. */
    FLValue FLDict_GetWithCFString(FLDict, CFStringRef);


#ifdef __OBJC__

    // Equivalents of the above functions that take & return Objective-C object types:
    
    /** Writes a Core Foundation (or Objective-C) object to an Encoder.
        Supports all the JSON types, as well as CFData. */
    bool FLEncoder_WriteNSObject(FLEncoder, id);


    /** Creates an NSMapTable configured for storing shared NSStrings for Fleece decoding. */
    NSMapTable* FLCreateSharedStringsTable(void);

    
    /** Returns a Value as a corresponding (autoreleased) Foundation object. */
    id FLValue_GetNSObject(FLValue, FLSharedKeys, NSMapTable *sharedStrings);


    /** Same as FLDictGet, but takes the key as an NSString. */
    FLValue FLDict_GetWithNSString(FLDict, NSString*);


    /** Same as FLEncoder_Finish, but returns result as NSData or error as NSError. */
    NSData* FLEncoder_FinishWithNSData(FLEncoder, NSError**);


    /** NSError domain string for Fleece errors */
    extern NSString* const FLErrorDomain;

#endif


#ifdef __cplusplus
}
#endif
