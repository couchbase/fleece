//
// FLValue.h
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
#ifndef _FLVALUE_H
#define _FLVALUE_H

#include "FLBase.h"

FL_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

    // This is the C API! For the C++ API, see Fleece.hh.


    /** \defgroup FLValue   Fleece Values
         @{
        The core Fleece data type is FLValue: a reference to a value in Fleece-encoded data.
        An FLValue can represent any JSON type (plus binary data).

        - Scalar data types -- numbers, booleans, null, strings, data -- can be accessed
          using individual functions of the form `FLValue_As...`; these return the scalar value,
          or a default zero/false/null value if the value is not of that type.
        - Collections -- arrays and dictionaries -- have their own "subclasses": FLArray and
          FLDict. These have the same pointer values as an FLValue but are not type-compatible
          in C. To coerce an FLValue to a collection type, call FLValue_AsArray or FLValue_AsDict.
          If the value is not of that type, NULL is returned. (FLArray and FLDict are documented
          fully in their own sections.)

        @note It's safe to pass a `NULL` pointer to an `FLValue`, `FLArray` or `FLDict`
              function parameter, except where specifically noted.
        @note Conversion/accessor functions that take `FLValue` won't complain if the value isn't
              of the desired subtype; they'll just return some default like 0 or `NULL`.
              For example, \ref FLValue_AsInt will return 0 if passed a non-integer value or NULL.*/

    /** Types of Fleece values. Basically JSON, with the addition of Data (raw blob). */
    typedef enum {
        kFLUndefined = -1,  /**< Type of a NULL pointer, i.e. no such value, like JSON `undefined`.
                                 Also the type of \ref kFLUndefinedValue, and of a value created by
                                 \ref FLEncoder_WriteUndefined(). */
        kFLNull = 0,        ///< Equivalent to a JSON 'null'
        kFLBoolean,         ///< A `true` or `false` value
        kFLNumber,          ///< A numeric value, either integer or floating-point
        kFLString,          ///< A string
        kFLData,            ///< Binary data (no JSON equivalent)
        kFLArray,           ///< An array of values
        kFLDict             ///< A mapping of strings to values (AKA "object" in JSON.)
    } FLValueType;


    /** A constant null value (like a JSON `null`, not a NULL pointer!) */
    FLEECE_PUBLIC extern const FLValue kFLNullValue;

    /** A constant undefined value. This is not a NULL pointer, but its type is \ref kFLUndefined.
        It can be stored in an \ref FLMutableArray or \ref FLMutableDict if you really, really
        need to store an undefined/empty value, not just a JSON `null`. */
    FLEECE_PUBLIC extern const FLValue kFLUndefinedValue;


    /** \name Accessors
        @{ */

     /** Returns the data type of an arbitrary value.
        If the parameter is a NULL pointer, returns `kFLUndefined`. */
    FLEECE_PUBLIC FLValueType FLValue_GetType(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns true if the value is non-NULL and represents an integer. */
    FLEECE_PUBLIC bool FLValue_IsInteger(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns true if the value is non-NULL and represents an integer >= 2^63. Such a value can't
        be represented in C as an `int64_t`, only a `uint64_t`, so you should access it by calling
        `FLValueAsUnsigned`, _not_ FLValueAsInt, which would return  an incorrect (negative)
        value. */
    FLEECE_PUBLIC bool FLValue_IsUnsigned(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns true if the value is non-NULL and represents a 64-bit floating-point number. */
    FLEECE_PUBLIC bool FLValue_IsDouble(FLValue FL_NULLABLE) FLAPI;

    /** Returns a value coerced to boolean. This will be true unless the value is NULL (undefined),
        null, false, or zero. */
    FLEECE_PUBLIC bool FLValue_AsBool(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns a value coerced to an integer. True and false are returned as 1 and 0, and
        floating-point numbers are rounded. All other types are returned as 0.
        @warning  Large 64-bit unsigned integers (2^63 and above) will come out wrong. You can
        check for these by calling `FLValueIsUnsigned`. */
    FLEECE_PUBLIC int64_t FLValue_AsInt(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns a value coerced to an unsigned integer.
        This is the same as `FLValueAsInt` except that it _can't_ handle negative numbers, but
        does correctly return large `uint64_t` values of 2^63 and up. */
    FLEECE_PUBLIC uint64_t FLValue_AsUnsigned(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns a value coerced to a 32-bit floating point number.
        True and false are returned as 1.0 and 0.0, and integers are converted to float. All other
        types are returned as 0.0.
        @warning  Large integers (outside approximately +/- 2^23) will lose precision due to the
        limitations of IEEE 32-bit float format. */
    FLEECE_PUBLIC float FLValue_AsFloat(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns a value coerced to a 32-bit floating point number.
        True and false are returned as 1.0 and 0.0, and integers are converted to float. All other
        types are returned as 0.0.
        @warning  Very large integers (outside approximately +/- 2^50) will lose precision due to
        the limitations of IEEE 32-bit float format. */
    FLEECE_PUBLIC double FLValue_AsDouble(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns the exact contents of a string value, or null for all other types. */
    FLEECE_PUBLIC FLString FLValue_AsString(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Converts a value to a timestamp, in milliseconds since Unix epoch, or INT64_MIN on failure.
        - A string is parsed as ISO-8601 (standard JSON date format).
        - A number is interpreted as a timestamp and returned as-is. */
    FLEECE_PUBLIC FLTimestamp FLValue_AsTimestamp(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns the exact contents of a data value, or null for all other types. */
    FLEECE_PUBLIC FLSlice FLValue_AsData(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** If a FLValue represents an array, returns it cast to FLArray, else NULL. */
    FLEECE_PUBLIC FLArray FL_NULLABLE FLValue_AsArray(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** If a FLValue represents a dictionary, returns it as an FLDict, else NULL. */
    FLEECE_PUBLIC FLDict FL_NULLABLE FLValue_AsDict(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns a string representation of any scalar value. Data values are returned in raw form.
        Arrays and dictionaries don't have a representation and will return NULL. */
    FLEECE_PUBLIC FLStringResult FLValue_ToString(FLValue FL_NULLABLE) FLAPI;

    /** Compares two values for equality. This is a deep recursive comparison. */
    FLEECE_PUBLIC bool FLValue_IsEqual(FLValue FL_NULLABLE v1, FLValue FL_NULLABLE v2) FLAPI FLPURE;

    /** Returns true if the value is mutable. */
    FLEECE_PUBLIC bool FLValue_IsMutable(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** @} */


    /** \name Reference-Counting
        Retaining a value extends its lifespan (and that of any values contained in it) until
        at least such time that it's released.
        - If the value comes from an \ref FLDoc, the doc's ref-count will be incremented.
        - If the value is mutable (heap-based), it has its own ref-count that will be incremented.
        @warning  Values obtained from \ref FLValue_FromData don't match either of those critera.
                  Their lifespan is entirely determined by the caller-provided data pointer, so
                  the retain call can't do anything about it. In this situation Fleece will throw
                  an exception like "Can't retain immutable Value that's not part of a Doc."
         @{ */

    /** Increments the ref-count of a mutable value, or of an immutable value's \ref FLDoc.
        @warning It is illegal to call this on a value obtained from \ref FLValue_FromData. */
    FLEECE_PUBLIC FLValue FL_NULLABLE FLValue_Retain(FLValue FL_NULLABLE) FLAPI;

    /** Decrements the ref-count of a mutable value, or of an immutable value's \ref FLDoc.
        If the ref-count reaches zero the corresponding object is freed.
        @warning It is illegal to call this on a value obtained from \ref FLValue_FromData. */
    FLEECE_PUBLIC void FLValue_Release(FLValue FL_NULLABLE) FLAPI;

    static inline FLArray FL_NULLABLE FLArray_Retain(FLArray FL_NULLABLE v) {FLValue_Retain((FLValue)v); return v;}
    static inline void FLArray_Release(FLArray FL_NULLABLE v)   {FLValue_Release((FLValue)v);}
    static inline FLDict FL_NULLABLE FLDict_Retain(FLDict FL_NULLABLE v)    {FLValue_Retain((FLValue)v); return v;}
    static inline void FLDict_Release(FLDict FL_NULLABLE v)     {FLValue_Release((FLValue)v);}

    /** @} */

    /** @} */

#ifdef __cplusplus
}
#endif

FL_ASSUME_NONNULL_END

#endif // _FLVALUE_H
