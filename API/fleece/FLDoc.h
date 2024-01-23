//
// FLDoc.h
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
#ifndef _FLDOC_H
#define _FLDOC_H

#include "FLBase.h"

FL_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

    // This is the C API! For the C++ API, see Fleece.hh.


    /** \defgroup reading   Reading Fleece Data
         @{
        \name FLDoc
         @{
            An FLDoc points to (and often owns) Fleece-encoded data and provides access to its
            Fleece values.
     */


    /** Creates an FLDoc from Fleece-encoded data that's been returned as a result from
        FLSlice_Copy or other API. The resulting document retains the data, so you don't need to
        worry about it remaining valid. */
    NODISCARD FLEECE_PUBLIC FLDoc FLDoc_FromResultData(FLSliceResult data, FLTrust,
                               FLSharedKeys FL_NULLABLE, FLSlice externData) FLAPI;

    /** Releases a reference to an FLDoc. This must be called once to free an FLDoc you created. */
    FLEECE_PUBLIC void FLDoc_Release(FLDoc FL_NULLABLE) FLAPI;

    /** Adds a reference to an FLDoc. This extends its lifespan until at least such time as you
        call FLRelease to remove the reference. */
    FLEECE_PUBLIC FLDoc FLDoc_Retain(FLDoc FL_NULLABLE) FLAPI;

    /** Returns the encoded Fleece data backing the document. */
    FLEECE_PUBLIC FLSlice FLDoc_GetData(FLDoc FL_NULLABLE) FLAPI FLPURE;

    /** Returns the FLSliceResult data owned by the document, if any, else a null slice. */
    FLEECE_PUBLIC FLSliceResult FLDoc_GetAllocedData(FLDoc FL_NULLABLE) FLAPI FLPURE;

    /** Returns the root value in the FLDoc, usually an FLDict. */
    FLEECE_PUBLIC FLValue FLDoc_GetRoot(FLDoc FL_NULLABLE) FLAPI FLPURE;

    /** Returns the FLSharedKeys used by this FLDoc, as specified when it was created. */
    FLEECE_PUBLIC FLSharedKeys FLDoc_GetSharedKeys(FLDoc FL_NULLABLE) FLAPI FLPURE;

    /** Looks up the Doc containing the Value, or NULL if there is none.
        @note Caller must release the FLDoc reference!! */
    NODISCARD FLEECE_PUBLIC FLDoc FL_NULLABLE FLValue_FindDoc(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Associates an arbitrary pointer value with a document, and thus its contained values.
        Allows client code to associate its own pointer with this FLDoc and its Values,
        which can later be retrieved with \ref FLDoc_GetAssociated.
        For example, this could be a pointer to an `app::Document` object, of which this Doc's
        root FLDict is its properties. You would store it by calling
        `FLDoc_SetAssociated(doc, myDoc, "app::Document");`.
        @param doc  The FLDoc to store a pointer in.
        @param pointer  The pointer to store in the FLDoc.
        @param type  A C string literal identifying the type. This is used to avoid collisions
                     with unrelated code that might try to store a different type of value.
        @return  True if the pointer was stored, false if a pointer of a different type is
                 already stored.
        @warning  Be sure to clear this before the associated object is freed/invalidated!
        @warning  This function is not thread-safe. Do not concurrently get & set objects. */
    FLEECE_PUBLIC bool FLDoc_SetAssociated(FLDoc FL_NULLABLE doc,
                             void * FL_NULLABLE pointer,
                             const char *type) FLAPI;

    /** Returns the pointer associated with the document. You can use this together with
        \ref FLValue_FindDoc to associate your own object with Fleece values, for instance to find
        your object that "owns" a value:
        `myDoc = (app::Document*)FLDoc_GetAssociated(FLValue_FindDoc(val), "app::Document");`.
        @param doc  The FLDoc to get a pointer from.
        @param type  The type of object expected, i.e. the same string literal passed to
                     \ref FLDoc_SetAssociated.
        @return  The associated pointer of that type, if any. */
    FLEECE_PUBLIC void* FLDoc_GetAssociated(FLDoc FL_NULLABLE doc, const char *type) FLAPI FLPURE;


    /** @} */
    /** @} */

#ifdef __cplusplus
}
#endif

FL_ASSUME_NONNULL_END

#endif // _FLDOC_H
