//
// FLKeyPath.h
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
#ifndef _FLKEYPATH_H
#define _FLKEYPATH_H

#include "FLBase.h"

FL_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

    // This is the C API! For the C++ API, see Fleece.hh.


    /** \defgroup FLKeyPath   Fleece Paths
        @{
     An FLKeyPath Describes a location in a Fleece object tree, as a path from the root that follows
     dictionary properties and array elements.
     It's similar to a JSONPointer or an Objective-C KeyPath, but simpler (so far.)
     The path is compiled into an efficient form that can be traversed quickly.

     It looks like `foo.bar[2][-3].baz` -- that is, properties prefixed with a `.`, and array
     indexes in brackets. (Negative indexes count from the end of the array.)

     A leading JSONPath-like `$.` is allowed but ignored.

     A '\' can be used to escape a special character ('.', '[' or '$').
     */

#ifndef FL_IMPL
    typedef struct _FLKeyPath*     FLKeyPath;       ///< A reference to a key path.
#endif

    /** Creates a new FLKeyPath object by compiling a path specifier string. */
    NODISCARD FLEECE_PUBLIC FLKeyPath FL_NULLABLE FLKeyPath_New(FLSlice specifier,
                                                                FLError* FL_NULLABLE outError) FLAPI;

    /** Frees a compiled FLKeyPath object. (It's ok to pass NULL.) */
    FLEECE_PUBLIC void FLKeyPath_Free(FLKeyPath FL_NULLABLE) FLAPI;

    /** Evaluates a compiled key-path for a given Fleece root object. */
    NODISCARD FLEECE_PUBLIC FLValue FL_NULLABLE FLKeyPath_Eval(FLKeyPath,
                                                               FLValue root) FLAPI;

    /** Evaluates a key-path from a specifier string, for a given Fleece root object.
        If you only need to evaluate the path once, this is a bit faster than creating an
        FLKeyPath object, evaluating, then freeing it. */
    NODISCARD FLEECE_PUBLIC FLValue FL_NULLABLE FLKeyPath_EvalOnce(FLSlice specifier, FLValue root,
                                                                   FLError* FL_NULLABLE outError) FLAPI;

    /** Returns a path in string form. */
    NODISCARD FLEECE_PUBLIC FLStringResult FLKeyPath_ToString(FLKeyPath path) FLAPI;

    /** Equality test. */
    FLEECE_PUBLIC bool FLKeyPath_Equals(FLKeyPath path1, FLKeyPath path2) FLAPI;

    /** The number of path components. */
    FLEECE_PUBLIC size_t FLKeyPath_GetCount(FLKeyPath) FLAPI;

    /** Returns an element of a path, either a key or an array index.
        @param path  The path to examine.
        @param i  The index of the component to examine.
        @param outDictKey  On return this will be the property name, or a null slice
                           if this component is an array index.
        @param outArrayIndex  On return this will be the array index,
                              or 0 if this component is a property.
        @returns  True on success, false if there is no such component. */
    FLEECE_PUBLIC bool FLKeyPath_GetElement(FLKeyPath path,
                                            size_t i,
                                            FLSlice *outDictKey,
                                            int32_t *outArrayIndex) FLAPI;

    /** Creates a new _empty_ FLKeyPath, for the purpose of adding components to it. */
    NODISCARD FLEECE_PUBLIC FLKeyPath FL_NULLABLE FLKeyPath_NewEmpty(void) FLAPI;

    /** Appends a single property/key component to a path. The string should not be escaped. */
    FLEECE_PUBLIC void FLKeyPath_AddProperty(FLKeyPath, FLString property) FLAPI;

    /** Appends a single array index component to a path. */
    FLEECE_PUBLIC void FLKeyPath_AddIndex(FLKeyPath, int index) FLAPI;

    /** Appends one or more components, encoded as a specifier like the one passed to FLKeyPath_New. */
    NODISCARD FLEECE_PUBLIC bool FLKeyPath_AddComponents(FLKeyPath, FLString specifier,
                                                         FLError* FL_NULLABLE outError) FLAPI;

    /** Removes the first `n` components. */
    FLEECE_PUBLIC void FLKeyPath_DropComponents(FLKeyPath, size_t n) FLAPI;

    /** @} */

#ifdef __cplusplus
}
#endif

FL_ASSUME_NONNULL_END

#endif // _FLKEYPATH_H
