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
#include "fleece/FLCollections.h"

#ifdef __OBJC__
#import <Foundation/NSMapTable.h>
#endif

FL_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

    /** \defgroup CF    Fleece CoreFoundation and Objective-C Helpers
        @{ */

    /** Writes a Core Foundation (or Objective-C) object to an Encoder.
        Supports all the JSON types, as well as CFData. */
    NODISCARD FLEECE_PUBLIC bool FLEncoder_WriteCFObject(FLEncoder, CFTypeRef) FLAPI;


    /** Returns a Value as a corresponding CoreFoundation object.
        Caller must CFRelease the result. */
    NODISCARD FLEECE_PUBLIC CFTypeRef FLValue_CopyCFObject(FLValue FL_NULLABLE) FLAPI;

    /** Copies a CoreFoundation object to a standalone Fleece object, if possible.
        @throws NSException if the object is not of a class that can be converted, or if it
                contains such an object.
        \note  You must call \ref FLValue_Release when finished with the result. */
    FLValue FLValue_FromCFValue(CFTypeRef);

    /** Stores a CoreFoundation object into a slot in a Fleece dict/array, if possible.
        This is supported for the CF/NS equivalents of Fleece types: CFString, CFNumber,
        CFData, CFArray, CFDictionary. */
    void FLSlot_SetCFValue(FLSlot slot, CFTypeRef value);

    /** Same as FLDictGet, but takes the key as a CFStringRef. */
    NODISCARD FLEECE_PUBLIC FLValue FLDict_GetWithCFString(FLDict FL_NULLABLE, CFStringRef) FLAPI;


#ifdef __OBJC__
    // Equivalents of the above functions that take & return Objective-C object types:
    
    /** Writes a Core Foundation (or Objective-C) object to an Encoder.
        Supports all the JSON types, as well as CFData. */
    FLEECE_PUBLIC bool FLEncoder_WriteNSObject(FLEncoder, id) FLAPI;


    /** Creates an NSMapTable configured for storing shared NSStrings for Fleece decoding. */
    FLEECE_PUBLIC NSMapTable* FLCreateSharedStringsTable(void) FLAPI;

    
    /** Returns a Value as a corresponding (autoreleased) Foundation object. */
    FLEECE_PUBLIC id FLValue_GetNSObject(FLValue FL_NULLABLE, NSMapTable* FL_NULLABLE sharedStrings) FLAPI;


    /** Same as FLDictGet, but takes the key as an NSString. */
    FLEECE_PUBLIC FLValue FLDict_GetWithNSString(FLDict FL_NULLABLE, NSString*) FLAPI;


    /** Returns an FLDictIterator's current key as an NSString. */
    FLEECE_PUBLIC NSString* FLDictIterator_GetKeyAsNSString(const FLDictIterator *i,
                                              NSMapTable* FL_NULLABLE sharedStrings) FLAPI;

    /** Same as FLEncoder_Finish, but returns result as NSData or error as NSError. */
    FLEECE_PUBLIC NSData* FLEncoder_FinishWithNSData(FLEncoder, NSError** FL_NULLABLE) FLAPI;

    /** Copies a Foundation object to a standalone Fleece object, if possible.
        @throws NSException if the object is not of a class that can be converted, or if it
                contains such an object.
        \note  You must call \ref FLValue_Release when finished with the result. */
    FLValue FLValue_FromNSObject(id);

    /** NSError domain string for Fleece errors */
    FLEECE_PUBLIC extern NSString* const FLErrorDomain;


    @interface NSObject (Fleece)
    /** This method is called on objects being encoded by
        FLEncoder_WriteNSObject (even recursively) if the encoder doesn't know how to encode
        them. You can implement this method in your classes. In it, call the encoder to write
        a single object (which may of course be an array or dictionary.) */
    - (void) fl_encodeToFLEncoder: (FLEncoder)enc;

    /** This method is called by \ref FLValue_FromNSObject and \ref FLValue_FromCFObject.
        It's already implemented by NSString, NSNumber, NSData, NSArray, NSDictionary.
        You can implement it in your own classes.
        @return  A non-NULL retained Fleece value, i.e. one that the caller of this method will
                have to call a "release" function on. Typically you'll create a mutable Fleece dict
                or array, populate it, and return it without releasing it. */
    - (FLValue) fl_convertToFleece;

    /** This method is called by \ref FLSlot_SetCFValue.
        It's already implemented by NSString, NSNumber, NSData, NSArray, NSDictionary.
        Otherwise, the default implementation calls `-fl_convertToFleece` and stores the
        resulting `FLValue` in the slot.
        You could override this, but it's not usually useful. */
    - (void) fl_storeInSlot: (FLSlot)slot;

    @end
#endif

/** @} */

#ifdef __cplusplus
}
#endif

FL_ASSUME_NONNULL_END
