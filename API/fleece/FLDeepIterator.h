//
// FLDeepIterator.h
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
#ifndef _FLDEEPITERATOR_H
#define _FLDEEPITERATOR_H

#include "FLBase.h"

FL_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

    // This is the C API! For the C++ API, see Fleece.hh.


    /** \defgroup FLDeepIterator   Fleece Deep Iterator
        @{
        A deep iterator traverses every value contained in a dictionary, in depth-first order.
        You can skip any nested collection by calling \ref FLDeepIterator_SkipChildren. */

#ifndef FL_IMPL
    typedef struct _FLDeepIterator* FLDeepIterator; ///< A reference to a deep iterator.
#endif

    /** Creates a FLDeepIterator to iterate over a dictionary.
        Call FLDeepIterator_GetKey and FLDeepIterator_GetValue to get the first item,
        then FLDeepIterator_Next. */
    FLEECE_PUBLIC FLDeepIterator FLDeepIterator_New(FLValue FL_NULLABLE) FLAPI;

    FLEECE_PUBLIC void FLDeepIterator_Free(FLDeepIterator FL_NULLABLE) FLAPI;

    /** Returns the current value being iterated over. or NULL at the end of iteration. */
    FLEECE_PUBLIC FLValue FL_NULLABLE FLDeepIterator_GetValue(FLDeepIterator) FLAPI;

    /** Returns the parent/container of the current value, or NULL at the end of iteration. */
    FLEECE_PUBLIC FLValue FL_NULLABLE FLDeepIterator_GetParent(FLDeepIterator) FLAPI;

    /** Returns the key of the current value in its parent, or an empty slice if not in a dictionary. */
    FLEECE_PUBLIC FLSlice FLDeepIterator_GetKey(FLDeepIterator) FLAPI;

    /** Returns the array index of the current value in its parent, or 0 if not in an array. */
    FLEECE_PUBLIC uint32_t FLDeepIterator_GetIndex(FLDeepIterator) FLAPI;

    /** Returns the current depth in the hierarchy, starting at 1 for the top-level children. */
    FLEECE_PUBLIC size_t FLDeepIterator_GetDepth(FLDeepIterator) FLAPI;

    /** Tells the iterator to skip the children of the current value. */
    FLEECE_PUBLIC void FLDeepIterator_SkipChildren(FLDeepIterator) FLAPI;

    /** Advances the iterator to the next value, or returns false if at the end. */
    FLEECE_PUBLIC bool FLDeepIterator_Next(FLDeepIterator) FLAPI;

    typedef struct {
        FLSlice key;        ///< Dict key, or kFLSliceNull if none
        uint32_t index;     ///< Array index, only if there's no key
    } FLPathComponent;

    /** Returns the path as an array of FLPathComponents. */
    FLEECE_PUBLIC void FLDeepIterator_GetPath(FLDeepIterator,
                                FLPathComponent* FL_NONNULL * FL_NONNULL outPath,
                                size_t* outDepth) FLAPI;

    /** Returns the current path in JavaScript format. */
    FLEECE_PUBLIC FLSliceResult FLDeepIterator_GetPathString(FLDeepIterator) FLAPI;

    /** Returns the current path in JSONPointer format (RFC 6901). */
    FLEECE_PUBLIC FLSliceResult FLDeepIterator_GetJSONPointer(FLDeepIterator) FLAPI;

    /** @} */

#ifdef __cplusplus
}
#endif

FL_ASSUME_NONNULL_END

#endif // _FLDEEPITERATOR_H
