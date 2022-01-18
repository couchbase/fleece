//
// Fleece.h
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
#ifndef _FLEECE_H
#define _FLEECE_H

#include "FLSlice.h"
#include <stdio.h>

// On Windows, FLEECE_PUBLIC marks symbols as being exported from the shared library.
// However, this is not the whole list of things that are exported.  The API methods
// are exported using a definition list, but it is not possible to correctly include
// initialized global variables, so those need to be marked (both in the header and 
// implementation) with FLEECE_PUBLIC.  See kFLNullValue below and in Fleece.cc
// for an example.
#if defined(_MSC_VER)
#ifdef FLEECE_EXPORTS
#define FLEECE_PUBLIC __declspec(dllexport)
#else
#define FLEECE_PUBLIC __declspec(dllimport)
#endif
#else
#define FLEECE_PUBLIC
#endif

FL_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

    // This is the C API! For the C++ API, see Fleece.hh.


    //////// BASIC TYPES

    /** \defgroup types    Basic Fleece Data Types
        @{ */

#ifndef FL_IMPL
    typedef const struct _FLValue* FLValue;         ///< A reference to a value of any type.
    typedef const struct _FLArray* FLArray;         ///< A reference to an array value.
    typedef const struct _FLDict*  FLDict;          ///< A reference to a dictionary (map) value.
    typedef struct _FLSlot*        FLSlot;          ///< A reference to a mutable array/dict item
    typedef struct _FLArray*       FLMutableArray;  ///< A reference to a mutable array.
    typedef struct _FLDict*        FLMutableDict;   ///< A reference to a mutable dictionary.
    typedef struct _FLEncoder*     FLEncoder;       ///< A reference to an encoder.
#endif


    /** Error codes returned from some API calls. */
    typedef enum {
        kFLNoError = 0,
        kFLMemoryError,        // Out of memory, or allocation failed
        kFLOutOfRange,         // Array index or iterator out of range
        kFLInvalidData,        // Bad input data (NaN, non-string key, etc.)
        kFLEncodeError,        // Structural error encoding (missing value, too many ends, etc.)
        kFLJSONError,          // Error parsing JSON
        kFLUnknownValue,       // Unparseable data in a Value (corrupt? Or from some distant future?)
        kFLInternalError,      // Something that shouldn't happen
        kFLNotFound,           // Key not found
        kFLSharedKeysStateError, // Misuse of shared keys (not in transaction, etc.)
        kFLPOSIXError,
        kFLUnsupported,         // Operation is unsupported
    } FLError;


    //////// TIMESTAMPS


    /** \name Timestamps
         @{
            Fleece does not have a native type for dates or times; like JSON, they are represented
            as strings in ISO-8601 format, which look like "2008-08-07T05:18:51.589Z".

            They can also be represented more compactly as numbers, interpreted as milliseconds
            since the Unix epoch (midnight at January 1 1970, UTC.)
     */

    /** A point in time, expressed as milliseconds since the Unix epoch (1-1-1970 midnight UTC.) */
    typedef int64_t FLTimestamp;

    /** A value representing a missing timestamp; returned when a date cannot be parsed. */
    #define FLTimestampNone INT64_MIN

    /** Returns an FLTimestamp corresponding to the current time. */
    FLTimestamp FLTimestamp_Now(void);

    /** Formats a timestamp as a date-time string in ISO-8601 format.
        @note  See also \ref FLEncoder_WriteDateString, which writes a timestamp to an `FLEncoder`.
        @param timestamp  A time, given as milliseconds since the Unix epoch (1/1/1970 00:00 UTC.)
        @param asUTC  If true, the timestamp will be given in universal time; if false, in the
                      local timezone.
        @return  A heap-allocated string, which you are responsible for releasing. */
    FLStringResult FLTimestamp_ToString(FLTimestamp timestamp, bool asUTC) FLAPI;

    /** Parses an ISO-8601 date-time string to a timestamp. On failure returns `FLTimestampNone`.
        @note  See also \ref FLValue_AsTimestamp, which takes an `FLValue` and interprets numeric
        representations as well as strings. */
    FLTimestamp FLTimestamp_FromString(FLString str) FLAPI;

    /** @} */


    //////// DOCUMENT


    /** @} */
    /** \defgroup reading   Reading Fleece Data
         @{
        \name FLDoc
         @{
            An FLDoc points to (and often owns) Fleece-encoded data and provides access to its
            Fleece values.
     */

#ifndef FL_IMPL
    typedef struct _FLDoc*         FLDoc;           ///< A reference to a document.
    typedef struct _FLSharedKeys*  FLSharedKeys;    ///< A reference to a shared-keys mapping.
#endif

    /** Specifies whether not input data is trusted to be 100% valid Fleece. */
    typedef enum {
        /** Input data is not trusted to be valid, and will be fully validated by the API call. */
        kFLUntrusted,
        /** Input data is trusted to be valid. The API will perform only minimal validation when
            reading it. This is faster than kFLUntrusted, but should only be used if
            the data was generated by a trusted encoder and has not been altered or corrupted. For
            example, this can be used to parse Fleece data previously stored by your code in local
            storage.
            If invalid data is read by this call, subsequent calls to Value accessor functions can
            crash or return bogus results (including data from arbitrary memory locations.) */
        kFLTrusted
    } FLTrust;


    /** Creates an FLDoc from Fleece-encoded data that's been returned as a result from
        FLSlice_Copy or other API. The resulting document retains the data, so you don't need to
        worry about it remaining valid. */
    FLDoc FLDoc_FromResultData(FLSliceResult data, FLTrust,
                               FLSharedKeys FL_NULLABLE, FLSlice externData) FLAPI;

    /** Creates an FLDoc from JSON-encoded data. The data is first encoded into Fleece, and the
        Fleece data is kept by the doc; the input JSON data is no longer needed after this
        function returns. */
    FLDoc FLDoc_FromJSON(FLSlice json, FLError* FL_NULLABLE outError) FLAPI;

    /** Releases a reference to an FLDoc. This must be called once to free an FLDoc you created. */
    void FLDoc_Release(FLDoc FL_NULLABLE) FLAPI;

    /** Adds a reference to an FLDoc. This extends its lifespan until at least such time as you
        call FLRelease to remove the reference. */
    FLDoc FLDoc_Retain(FLDoc FL_NULLABLE) FLAPI;

    /** Returns the encoded Fleece data backing the document. */
    FLSlice FLDoc_GetData(FLDoc FL_NULLABLE) FLAPI FLPURE;

    /** Returns the FLSliceResult data owned by the document, if any, else a null slice. */
    FLSliceResult FLDoc_GetAllocedData(FLDoc FL_NULLABLE) FLAPI FLPURE;

    /** Returns the root value in the FLDoc, usually an FLDict. */
    FLValue FLDoc_GetRoot(FLDoc FL_NULLABLE) FLAPI FLPURE;

    /** Returns the FLSharedKeys used by this FLDoc, as specified when it was created. */
    FLSharedKeys FLDoc_GetSharedKeys(FLDoc FL_NULLABLE) FLAPI FLPURE;

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
    bool FLDoc_SetAssociated(FLDoc FL_NULLABLE doc,
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
    void* FLDoc_GetAssociated(FLDoc FL_NULLABLE doc, const char *type) FLAPI FLPURE;

    /** Looks up the Doc containing the Value, or NULL if the Value was created without a Doc.
        @note Caller must release the FLDoc reference!! */
    FLDoc FL_NULLABLE FLValue_FindDoc(FLValue FL_NULLABLE) FLAPI FLPURE;


    /** @} */
    /** \name Parsing And Converting Values Directly
     @{ */

    /** Returns a pointer to the root value in the encoded data, or NULL if validation failed.
        The FLValue, and all values found through it, are only valid as long as the encoded data
        remains intact and unchanged. */
    FLValue FL_NULLABLE FLValue_FromData(FLSlice data, FLTrust) FLAPI FLPURE;

    /** Directly converts JSON data to Fleece-encoded data.
        You can then call FLValue_FromData (in kFLTrusted mode) to get the root as a Value. */
    FLSliceResult FLData_ConvertJSON(FLSlice json, FLError* FL_NULLABLE outError) FLAPI;

    /** Produces a human-readable dump of the Value encoded in the data.
        This is only useful if you already know, or want to learn, the encoding format. */
    FLStringResult FLData_Dump(FLSlice data) FLAPI;


    /** @} */
    /** @} */
    /** \defgroup json   Converting Fleece To JSON
     @{
        These are convenience functions that directly return JSON-encoded output.
        For more control over the encoding, use an FLEncoder. */

    /** Encodes a Fleece value as JSON (or a JSON fragment.)
        Any Data values will become base64-encoded JSON strings. */
    FLStringResult FLValue_ToJSON(FLValue FL_NULLABLE) FLAPI;

    /** Encodes a Fleece value as JSON5, a more lenient variant of JSON that allows dictionary
        keys to be unquoted if they're alphanumeric. This tends to be more readable. */
    FLStringResult FLValue_ToJSON5(FLValue FL_NULLABLE) FLAPI;

    /** Most general Fleece to JSON converter. */
    FLStringResult FLValue_ToJSONX(FLValue FL_NULLABLE v,
                                   bool json5,
                                   bool canonicalForm) FLAPI;

    /** Converts valid JSON5 <https://json5.org> to JSON. Among other things, it converts single
        quotes to double, adds missing quotes around dictionary keys, removes trailing commas,
        and removes comments.
        @note If given invalid JSON5, it will _usually_ return an error, but may just ouput
              comparably invalid JSON, in which case the caller's subsequent JSON parsing will
              detect the error. The types of errors it overlooks tend to be subtleties of string
              or number encoding.
        @param json5  The JSON5 to parse
        @param outErrorMessage  On failure, the error message will be stored here (if not NULL.)
                        As this is a \ref FLStringResult, you will be responsible for freeing it.
        @param outErrorPos  On a parse error, the byte offset in the input where the error occurred
                        will be stored here (if it's not NULL.)
        @param outError  On failure, the error code will be stored here (if it's not NULL.)
        @return  The converted JSON. */
    FLStringResult FLJSON5_ToJSON(FLString json5,
                                  FLStringResult* FL_NULLABLE outErrorMessage,
                                  size_t* FL_NULLABLE outErrorPos,
                                  FLError* FL_NULLABLE outError) FLAPI;

    /** \name Debugging Functions
        @{ */
    /** Debugging function that returns a C string of JSON. Does not free the string's memory! */
    const char* FL_NULLABLE FLDump(FLValue FL_NULLABLE) FLAPI;
    /** Debugging function that returns a C string of JSON. Does not free the string's memory! */
    const char* FL_NULLABLE FLDumpData(FLSlice data) FLAPI;

    /** @} */


    //////// VALUE


    /** @} */
    /** \defgroup FLValue   Fleece Value Accessors
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

        It's always safe to pass a NULL value to an accessor; that goes for FLDict and FLArray
        as well as FLValue. The result will be a default value of that type, e.g. false or 0
        or NULL, unless otherwise specified. */

    /** Types of Fleece values. Basically JSON, with the addition of Data (raw blob). */
    typedef enum {
        kFLUndefined = -1,  ///< Type of a NULL pointer, i.e. no such value, like JSON `undefined`. Also the type of a value created by FLEncoder_WriteUndefined().
        kFLNull = 0,        ///< Equivalent to a JSON 'null'
        kFLBoolean,         ///< A `true` or `false` value
        kFLNumber,          ///< A numeric value, either integer or floating-point
        kFLString,          ///< A string
        kFLData,            ///< Binary data (no JSON equivalent)
        kFLArray,           ///< An array of values
        kFLDict             ///< A mapping of strings to values
    } FLValueType;


    /** Returns the data type of an arbitrary Value.
        (If the parameter is a NULL pointer, returns `kFLUndefined`.) */
    FLValueType FLValue_GetType(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns true if the value is non-NULL and represents an integer. */
    bool FLValue_IsInteger(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns true if the value is non-NULL and represents an integer >= 2^63. Such a value can't
        be represented in C as an `int64_t`, only a `uint64_t`, so you should access it by calling
        `FLValueAsUnsigned`, _not_ FLValueAsInt, which would return  an incorrect (negative)
        value. */
    bool FLValue_IsUnsigned(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns true if the value is non-NULL and represents a 64-bit floating-point number. */
    bool FLValue_IsDouble(FLValue FL_NULLABLE) FLAPI;

    /** Returns a value coerced to boolean. This will be true unless the value is NULL (undefined),
        null, false, or zero. */
    bool FLValue_AsBool(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns a value coerced to an integer. True and false are returned as 1 and 0, and
        floating-point numbers are rounded. All other types are returned as 0.
        @warning  Large 64-bit unsigned integers (2^63 and above) will come out wrong. You can
        check for these by calling `FLValueIsUnsigned`. */
    int64_t FLValue_AsInt(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns a value coerced to an unsigned integer.
        This is the same as `FLValueAsInt` except that it _can't_ handle negative numbers, but
        does correctly return large `uint64_t` values of 2^63 and up. */
    uint64_t FLValue_AsUnsigned(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns a value coerced to a 32-bit floating point number.
        True and false are returned as 1.0 and 0.0, and integers are converted to float. All other
        types are returned as 0.0.
        @warning  Large integers (outside approximately +/- 2^23) will lose precision due to the
        limitations of IEEE 32-bit float format. */
    float FLValue_AsFloat(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns a value coerced to a 32-bit floating point number.
        True and false are returned as 1.0 and 0.0, and integers are converted to float. All other
        types are returned as 0.0.
        @warning  Very large integers (outside approximately +/- 2^50) will lose precision due to
        the limitations of IEEE 32-bit float format. */
    double FLValue_AsDouble(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns the exact contents of a string value, or null for all other types. */
    FLString FLValue_AsString(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Converts a value to a timestamp, in milliseconds since Unix epoch, or INT64_MIN on failure.
        - A string is parsed as ISO-8601 (standard JSON date format).
        - A number is interpreted as a timestamp and returned as-is. */
    FLTimestamp FLValue_AsTimestamp(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns the exact contents of a data value, or null for all other types. */
    FLSlice FLValue_AsData(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** If a FLValue represents an array, returns it cast to FLArray, else NULL. */
    FLArray FL_NULLABLE FLValue_AsArray(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** If a FLValue represents a dictionary, returns it as an FLDict, else NULL. */
    FLDict FL_NULLABLE FLValue_AsDict(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** Returns a string representation of any scalar value. Data values are returned in raw form.
        Arrays and dictionaries don't have a representation and will return NULL. */
    FLStringResult FLValue_ToString(FLValue FL_NULLABLE) FLAPI;

    /** Compares two values for equality. This is a deep recursive comparison. */
    bool FLValue_IsEqual(FLValue FL_NULLABLE v1, FLValue FL_NULLABLE v2) FLAPI FLPURE;

    /** Returns true if the value is mutable. */
    bool FLValue_IsMutable(FLValue FL_NULLABLE) FLAPI FLPURE;

    /** \name Ref-counting (mutable values only)
         @{ */

    /** If this value is mutable (and thus heap-based) its ref-count is incremented.
        Otherwise, this call does nothing. */
    FLValue FL_NULLABLE FLValue_Retain(FLValue FL_NULLABLE) FLAPI;

    /** If this value is mutable (and thus heap-based) its ref-count is decremented, and if it
        reaches zero the value is freed.
        If the value is not mutable, this call does nothing. */
    void FLValue_Release(FLValue FL_NULLABLE) FLAPI;

    static inline FLArray FL_NULLABLE FLArray_Retain(FLArray FL_NULLABLE v) {FLValue_Retain((FLValue)v); return v;}
    static inline void FLArray_Release(FLArray FL_NULLABLE v)   {FLValue_Release((FLValue)v);}
    static inline FLDict FL_NULLABLE FLDict_Retain(FLDict FL_NULLABLE v)    {FLValue_Retain((FLValue)v); return v;}
    static inline void FLDict_Release(FLDict FL_NULLABLE v)     {FLValue_Release((FLValue)v);}

    /** @} */

    /** Allocates a string value on the heap. This is rarely needed -- usually you'd just add a string
        to a mutable Array or Dict directly using one of their "...SetString" or "...AppendString"
        methods. */
    FLValue FL_NULLABLE FLValue_NewString(FLString) FLAPI;

    /** Allocates a data/blob value on the heap. This is rarely needed -- usually you'd just add data
        to a mutable Array or Dict directly using one of their "...SetData or "...AppendData"
        methods. */
    FLValue FL_NULLABLE FLValue_NewData(FLSlice) FLAPI;

    /** A constant null value (not a NULL pointer!) */
    FLEECE_PUBLIC extern const FLValue kFLNullValue;

    /** A constant undefined value */
    FLEECE_PUBLIC extern const FLValue kFLUndefinedValue;


    //////// ARRAY


    /** @} */
    /** \defgroup FLArray   Fleece Arrays
        @{
        FLArray is a "subclass" of FLValue, representing values that are arrays. It's always OK to
        pass an FLArray to a function parameter expecting an FLValue, even though the compiler
        makes you use an explicit type-cast. It's safe to type-cast the other direction, from
        FLValue to FLArray, _only_ if you already know that the value is an array, e.g. by having
        called FLValue_GetType on it. But it's safer to call FLValue_AsArray instead, since it
        will return NULL if the value isn't an array.
     */

    /** Returns the number of items in an array, or 0 if the pointer is NULL. */
    uint32_t FLArray_Count(FLArray FL_NULLABLE) FLAPI FLPURE;

    /** Returns true if an array is empty (or NULL). Depending on the array's representation,
        this can be faster than `FLArray_Count(a) == 0` */
    bool FLArray_IsEmpty(FLArray FL_NULLABLE) FLAPI FLPURE;

    /** If the array is mutable, returns it cast to FLMutableArray, else NULL. */
    FLMutableArray FL_NULLABLE FLArray_AsMutable(FLArray FL_NULLABLE) FLAPI FLPURE;

    /** Returns an value at an array index, or NULL if the index is out of range. */
    FLValue FL_NULLABLE FLArray_Get(FLArray FL_NULLABLE, uint32_t index) FLAPI FLPURE;

    FLEECE_PUBLIC extern const FLArray kFLEmptyArray;

    /** \name Array iteration
        @{
Iterating an array typically looks like this:

```
FLArrayIterator iter;
FLArrayIterator_Begin(theArray, &iter);
FLValue value;
while (NULL != (value = FLArrayIterator_GetValue(&iter))) {
  // ...
  FLArrayIterator_Next(&iter);
}
```
     */

    /** Opaque array iterator. Declare one on the stack and pass its address to
        `FLArrayIteratorBegin`. */
    typedef struct {
#if !DOXYGEN_PARSING
        void* _private1;
        uint32_t _private2;
        bool _private3;
        void* _private4;
#endif
    } FLArrayIterator;

    /** Initializes a FLArrayIterator struct to iterate over an array.
        Call FLArrayIteratorGetValue to get the first item, then FLArrayIteratorNext. */
    void FLArrayIterator_Begin(FLArray FL_NULLABLE, FLArrayIterator*) FLAPI;

    /** Returns the current value being iterated over. */
    FLValue FL_NULLABLE FLArrayIterator_GetValue(const FLArrayIterator*) FLAPI FLPURE;

    /** Returns a value in the array at the given offset from the current value. */
    FLValue FL_NULLABLE FLArrayIterator_GetValueAt(const FLArrayIterator*, uint32_t offset) FLAPI FLPURE;

    /** Returns the number of items remaining to be iterated, including the current one. */
    uint32_t FLArrayIterator_GetCount(const FLArrayIterator*) FLAPI FLPURE;

    /** Advances the iterator to the next value, or returns false if at the end. */
    bool FLArrayIterator_Next(FLArrayIterator*) FLAPI;

    /** @} */


    //////// MUTABLE ARRAY


    /** \name Mutable Arrays
         @{ */

    typedef enum {
        kFLDefaultCopy        = 0,
        kFLDeepCopy           = 1,
        kFLCopyImmutables     = 2,
        kFLDeepCopyImmutables = (kFLDeepCopy | kFLCopyImmutables),
    } FLCopyFlags;


    /** Creates a new mutable Array that's a copy of the source Array.
        Its initial ref-count is 1, so a call to FLMutableArray_Release will free it.

        Copying an immutable Array is very cheap (only one small allocation) unless the flag
        kFLCopyImmutables is set.

        Copying a mutable Array is cheap if it's a shallow copy, but if `deepCopy` is true,
        nested mutable Arrays and Dicts are also copied, recursively; if kFLCopyImmutables is
        also set, immutable values are also copied.

        If the source Array is NULL, then NULL is returned. */
    FLMutableArray FL_NULLABLE FLArray_MutableCopy(FLArray FL_NULLABLE, FLCopyFlags) FLAPI;

    /** Creates a new empty mutable Array.
        Its initial ref-count is 1, so a call to FLMutableArray_Release will free it.  */
    FLMutableArray FL_NULLABLE FLMutableArray_New(void) FLAPI;

    /** Creates a new mutable Array from JSON. The input json must represent a JSON array or NULL will be returned.
        Its initial ref-count is 1, so a call to FLMutableArray_Release will free it.  */
    FLMutableArray FL_NULLABLE FLMutableArray_NewFromJSON(FLString json, FLError* FL_NULLABLE outError) FLAPI;

    /** Increments the ref-count of a mutable Array. */
    static inline FLMutableArray FL_NULLABLE FLMutableArray_Retain(FLMutableArray FL_NULLABLE d) {
        return (FLMutableArray)FLValue_Retain((FLValue)d);
    }
    /** Decrements the refcount of (and possibly frees) a mutable Array. */
    static inline void FLMutableArray_Release(FLMutableArray FL_NULLABLE d) {
        FLValue_Release((FLValue)d);
    }

    /** If the Array was created by FLArray_MutableCopy, returns the original source Array. */
    FLArray FL_NULLABLE FLMutableArray_GetSource(FLMutableArray FL_NULLABLE) FLAPI;

    /** Returns true if the Array has been changed from the source it was copied from. */
    bool FLMutableArray_IsChanged(FLMutableArray FL_NULLABLE) FLAPI;

    /** Sets or clears the mutable Array's "changed" flag. */
    void FLMutableArray_SetChanged(FLMutableArray FL_NULLABLE, bool) FLAPI;

    /** Inserts a contiguous range of JSON `null` values into the array.
        @param array  The array to operate on.
        @param firstIndex  The zero-based index of the first value to be inserted.
        @param count  The number of items to insert. */
    void FLMutableArray_Insert(FLMutableArray FL_NULLABLE array, uint32_t firstIndex, uint32_t count) FLAPI;

    /** Removes contiguous items from the array.
        @param array  The array to operate on.
        @param firstIndex  The zero-based index of the first item to remove.
        @param count  The number of items to remove. */
    void FLMutableArray_Remove(FLMutableArray FL_NULLABLE array, uint32_t firstIndex, uint32_t count) FLAPI;

    /** Changes the size of an array.
        If the new size is larger, the array is padded with JSON `null` values.
        If it's smaller, values are removed from the end. */
    void FLMutableArray_Resize(FLMutableArray FL_NULLABLE array, uint32_t size) FLAPI;

    /** Convenience function for getting an array-valued property in mutable form.
        - If the value for the key is not an array, returns NULL.
        - If the value is a mutable array, returns it.
        - If the value is an immutable array, this function makes a mutable copy, assigns the
          copy as the property value, and returns the copy. */
    FLMutableArray FL_NULLABLE FLMutableArray_GetMutableArray(FLMutableArray FL_NULLABLE, uint32_t index) FLAPI;

    /** Convenience function for getting an array-valued property in mutable form.
        - If the value for the key is not an array, returns NULL.
        - If the value is a mutable array, returns it.
        - If the value is an immutable array, this function makes a mutable copy, assigns the
          copy as the property value, and returns the copy. */
    FLMutableDict FL_NULLABLE FLMutableArray_GetMutableDict(FLMutableArray FL_NULLABLE, uint32_t index) FLAPI;


    /// Stores a JSON null value into an array.
    static inline void FLMutableArray_SetNull(FLMutableArray, uint32_t index);
    /// Stores a boolean value into an array.
    static inline void FLMutableArray_SetBool(FLMutableArray, uint32_t index, bool);
    /// Stores an integer into an array.
    static inline void FLMutableArray_SetInt(FLMutableArray, uint32_t index, int64_t);
    /// Stores an unsigned integer into an array.
    /// \note: The only time this needs to be called, instead of \ref FLMutableArray_SetInt,
    ///        is if the value is greater than or equal to 2^63 and won't fit in an `int64_t`.
    static inline void FLMutableArray_SetUInt(FLMutableArray, uint32_t index, uint64_t);
    /// Stores a 32-bit floating-point number into an array.
    static inline void FLMutableArray_SetFloat(FLMutableArray, uint32_t index, float);
    /// Stores a 64-bit floating point number into an array.
    static inline void FLMutableArray_SetDouble(FLMutableArray, uint32_t index, double);
    /// Stores a UTF-8-encoded string into an array.
    static inline void FLMutableArray_SetString(FLMutableArray, uint32_t index, FLString);
    /// Stores a binary data blob into an array.
    static inline void FLMutableArray_SetData(FLMutableArray, uint32_t index, FLSlice);
    /// Stores a Fleece value into an array.
    static inline void FLMutableArray_SetValue(FLMutableArray, uint32_t index, FLValue);
    /// Stores a Fleece array into an array
    static inline void FLMutableArray_SetArray(FLMutableArray, uint32_t index, FLArray);
    /// Stores a Fleece dictionary into an array
    static inline void FLMutableArray_SetDict(FLMutableArray, uint32_t index, FLDict);

    /// Appends a JSON null value to an array.
    static inline void FLMutableArray_AppendNull(FLMutableArray);
    /// Appends a boolean value to an array.
    static inline void FLMutableArray_AppendBool(FLMutableArray, bool);
    /// Appends an integer to an array.
    static inline void FLMutableArray_AppendInt(FLMutableArray, int64_t);
    /// Appends an unsigned integer to an array.
    /// \note: The only time this needs to be called, instead of \ref FLMutableArray_AppendInt,
    ///        is if the value is greater than or equal to 2^63 and won't fit in an `int64_t`.
    static inline void FLMutableArray_AppendUInt(FLMutableArray, uint64_t);
    /// Appends a 32-bit floating-point number to an array.
    static inline void FLMutableArray_AppendFloat(FLMutableArray, float);
    /// Appends a 64-bit floating point number to an array.
    static inline void FLMutableArray_AppendDouble(FLMutableArray, double);
    /// Appends a UTF-8-encoded string to an array.
    static inline void FLMutableArray_AppendString(FLMutableArray, FLString);
    /// Appends a binary data blob to an array.
    static inline void FLMutableArray_AppendData(FLMutableArray, FLSlice);
    /// Appends a Fleece value to an array.
    static inline void FLMutableArray_AppendValue(FLMutableArray, FLValue);
    /// Appends a Fleece array to an array
    static inline void FLMutableArray_AppendArray(FLMutableArray, FLArray);
    /// Appends a Fleece dictionary to an array
    static inline void FLMutableArray_AppendDict(FLMutableArray, FLDict);


    /** @} */


    //////// DICT


    /** @} */
    /** \defgroup FLDict   Fleece Dictionaries
        @{ */

    /** Returns the number of items in a dictionary, or 0 if the pointer is NULL. */
    uint32_t FLDict_Count(FLDict FL_NULLABLE) FLAPI FLPURE;

    /** Returns true if a dictionary is empty (or NULL). Depending on the dictionary's
        representation, this can be faster than `FLDict_Count(a) == 0` */
    bool FLDict_IsEmpty(FLDict FL_NULLABLE) FLAPI FLPURE;

    /** If the dictionary is mutable, returns it cast to FLMutableDict, else NULL. */
    FLMutableDict FL_NULLABLE FLDict_AsMutable(FLDict FL_NULLABLE) FLAPI FLPURE;

    /** Looks up a key in a dictionary, returning its value.
        Returns NULL if the value is not found or if the dictionary is NULL. */
    FLValue FL_NULLABLE FLDict_Get(FLDict FL_NULLABLE, FLSlice keyString) FLAPI FLPURE;

    FLEECE_PUBLIC extern const FLDict kFLEmptyDict;

    /** \name Dict iteration
         @{
Iterating a dictionary typically looks like this:

```
FLDictIterator iter;
FLDictIterator_Begin(theDict, &iter);
FLValue value;
while (NULL != (value = FLDictIterator_GetValue(&iter))) {
    FLString key = FLDictIterator_GetKeyString(&iter);
    // ...
    FLDictIterator_Next(&iter);
}
```
     */

    /** Opaque dictionary iterator. Declare one on the stack, and pass its address to
        FLDictIterator_Begin. */
    typedef struct {
#if !DOXYGEN_PARSING
        void* _private1;
        uint32_t _private2;
        bool _private3;
        void *_private4, *_private5, *_private6, *_private7;
        int _private8;
#endif
    } FLDictIterator;

    /** Initializes a FLDictIterator struct to iterate over a dictionary.
        Call FLDictIterator_GetKey and FLDictIterator_GetValue to get the first item,
        then FLDictIterator_Next. */
    void FLDictIterator_Begin(FLDict FL_NULLABLE, FLDictIterator*) FLAPI;

    /** Returns the current key being iterated over. This Value will be a string or an integer. */
    FLValue FL_NULLABLE FLDictIterator_GetKey(const FLDictIterator*) FLAPI FLPURE;

    /** Returns the current key's string value. */
    FLString FLDictIterator_GetKeyString(const FLDictIterator*) FLAPI;

    /** Returns the current value being iterated over. */
    FLValue FL_NULLABLE FLDictIterator_GetValue(const FLDictIterator*) FLAPI FLPURE;

    /** Returns the number of items remaining to be iterated, including the current one. */
    uint32_t FLDictIterator_GetCount(const FLDictIterator*) FLAPI FLPURE;

    /** Advances the iterator to the next value, or returns false if at the end. */
    bool FLDictIterator_Next(FLDictIterator*) FLAPI;

    /** Cleans up after an iterator. Only needed if (a) the dictionary is a delta, and
        (b) you stop iterating before the end (i.e. before FLDictIterator_Next returns false.) */
    void FLDictIterator_End(FLDictIterator*) FLAPI;

    /** @} */
    /** \name Optimized Keys
        @{ */

    /** Opaque key for a dictionary. You are responsible for creating space for these; they can
        go on the stack, on the heap, inside other objects, anywhere. 
        Be aware that the lookup operations that use these will write into the struct to store
        "hints" that speed up future searches. */
    typedef struct {
#if !DOXYGEN_PARSING
        FLSlice _private1;
        void* _private2;
        uint32_t _private3, private4;
        bool private5;
#endif
    } FLDictKey;

    /** Initializes an FLDictKey struct with a key string.
        @warning  The input string's memory MUST remain valid for as long as the FLDictKey is in
        use! (The FLDictKey stores a pointer to the string, but does not copy it.)
        @param string  The key string (UTF-8).
        @return  An initialized FLDictKey struct. */
    FLDictKey FLDictKey_Init(FLSlice string) FLAPI;

    /** Returns the string value of the key (which it was initialized with.) */
    FLString FLDictKey_GetString(const FLDictKey*) FLAPI;

    /** Looks up a key in a dictionary using an FLDictKey. If the key is found, "hint" data will
        be stored inside the FLDictKey that will speed up subsequent lookups. */
    FLValue FL_NULLABLE FLDict_GetWithKey(FLDict FL_NULLABLE, FLDictKey*) FLAPI;


    //////// MUTABLE DICT


    /** @} */
    /** \name Mutable dictionaries
         @{ */

    /** Creates a new mutable Dict that's a copy of the source Dict.
        Its initial ref-count is 1, so a call to FLMutableDict_Release will free it.

        Copying an immutable Dict is very cheap (only one small allocation.) The `deepCopy` flag
        is ignored.

        Copying a mutable Dict is cheap if it's a shallow copy, but if `deepCopy` is true,
        nested mutable Dicts and Arrays are also copied, recursively.

        If the source dict is NULL, then NULL is returned. */
    FLMutableDict FL_NULLABLE FLDict_MutableCopy(FLDict FL_NULLABLE source, FLCopyFlags) FLAPI;

    /** Creates a new empty mutable Dict.
        Its initial ref-count is 1, so a call to FLMutableDict_Release will free it.  */
    FLMutableDict FL_NULLABLE FLMutableDict_New(void) FLAPI;

    /** Creates a new mutable Dict from json. The input JSON must represent a JSON array, or NULL will be returned.
        Its initial ref-count is 1, so a call to FLMutableDict_Release will free it.  */
    FLMutableDict FL_NULLABLE FLMutableDict_NewFromJSON(FLString json, FLError* FL_NULLABLE outError) FLAPI;

    /** Increments the ref-count of a mutable Dict. */
    static inline FLMutableDict FL_NULLABLE FLMutableDict_Retain(FLMutableDict FL_NULLABLE d) {
        return (FLMutableDict)FLValue_Retain((FLValue)d);
    }

    /** Decrements the refcount of (and possibly frees) a mutable Dict. */
    static inline void FLMutableDict_Release(FLMutableDict FL_NULLABLE d) {
        FLValue_Release((FLValue)d);
    }

    /** If the Dict was created by FLDict_MutableCopy, returns the original source Dict. */
    FLDict FL_NULLABLE FLMutableDict_GetSource(FLMutableDict FL_NULLABLE) FLAPI;

    /** Returns true if the Dict has been changed from the source it was copied from. */
    bool FLMutableDict_IsChanged(FLMutableDict FL_NULLABLE) FLAPI;

    /** Sets or clears the mutable Dict's "changed" flag. */
    void FLMutableDict_SetChanged(FLMutableDict FL_NULLABLE, bool) FLAPI;

    /** Removes the value for a key. */
    void FLMutableDict_Remove(FLMutableDict FL_NULLABLE, FLString key) FLAPI;

    /** Removes all keys and values. */
    void FLMutableDict_RemoveAll(FLMutableDict FL_NULLABLE) FLAPI;

    /** Convenience function for getting an array-valued property in mutable form.
        - If the value for the key is not an array, returns NULL.
        - If the value is a mutable array, returns it.
        - If the value is an immutable array, this function makes a mutable copy, assigns the
          copy as the property value, and returns the copy. */
    FLMutableArray FL_NULLABLE FLMutableDict_GetMutableArray(FLMutableDict FL_NULLABLE, FLString key) FLAPI;

    /** Convenience function for getting a dict-valued property in mutable form.
        - If the value for the key is not a dict, returns NULL.
        - If the value is a mutable dict, returns it.
        - If the value is an immutable dict, this function makes a mutable copy, assigns the
          copy as the property value, and returns the copy. */
    FLMutableDict FL_NULLABLE FLMutableDict_GetMutableDict(FLMutableDict FL_NULLABLE, FLString key) FLAPI;


    /// Stores a JSON null value into a mutable dictionary.
    static inline void FLMutableDict_SetNull(FLMutableDict, FLString key);
    /// Stores a boolean value into a mutable dictionary.
    static inline void FLMutableDict_SetBool(FLMutableDict, FLString key, bool);
    /// Stores an integer into a mutable dictionary.
    static inline void FLMutableDict_SetInt(FLMutableDict, FLString key, int64_t);
    /// Stores an unsigned integer into a mutable dictionary.
    /// \note: The only time this needs to be called, instead of \ref FLMutableDict_SetInt,
    ///        is if the value is greater than or equal to 2^63 and won't fit in an `int64_t`.
    static inline void FLMutableDict_SetUInt(FLMutableDict, FLString key, uint64_t);
    /// Stores a 32-bit floating-point number into a mutable dictionary.
    static inline void FLMutableDict_SetFloat(FLMutableDict, FLString key, float);
    /// Stores a 64-bit floating point number into a mutable dictionary.
    static inline void FLMutableDict_SetDouble(FLMutableDict, FLString key, double);
    /// Stores a UTF-8-encoded string into a mutable dictionary.
    static inline void FLMutableDict_SetString(FLMutableDict, FLString key, FLString);
    /// Stores a binary data blob into a mutable dictionary.
    static inline void FLMutableDict_SetData(FLMutableDict, FLString key, FLSlice);
    /// Stores a Fleece value into a mutable dictionary.
    static inline void FLMutableDict_SetValue(FLMutableDict, FLString key, FLValue);
    /// Stores a Fleece array into a mutable dictionary.
    static inline void FLMutableDict_SetArray(FLMutableDict, FLString key, FLArray);
    /// Stores a Fleece dictionary into a mutable dictionary.
    static inline void FLMutableDict_SetDict(FLMutableDict, FLString key, FLDict);


    /** @} */


    //////// DEEP ITERATOR


    /** @} */
    /** \defgroup FLDeepIterator   Fleece Deep Iterator
        @{
        A deep iterator traverses every value contained in a dictionary, in depth-first order.
        You can skip any nested collection by calling FLDeepIterator_SkipChildren. */

#ifndef FL_IMPL
    typedef struct _FLDeepIterator* FLDeepIterator; ///< A reference to a deep iterator.
#endif

    /** Creates a FLDeepIterator to iterate over a dictionary.
        Call FLDeepIterator_GetKey and FLDeepIterator_GetValue to get the first item,
        then FLDeepIterator_Next. */
    FLDeepIterator FLDeepIterator_New(FLValue FL_NULLABLE) FLAPI;

    void FLDeepIterator_Free(FLDeepIterator FL_NULLABLE) FLAPI;

    /** Returns the current value being iterated over. or NULL at the end of iteration. */
    FLValue FL_NULLABLE FLDeepIterator_GetValue(FLDeepIterator) FLAPI;

    /** Returns the parent/container of the current value, or NULL at the end of iteration. */
    FLValue FL_NULLABLE FLDeepIterator_GetParent(FLDeepIterator) FLAPI;

    /** Returns the key of the current value in its parent, or an empty slice if not in a dictionary. */
    FLSlice FLDeepIterator_GetKey(FLDeepIterator) FLAPI;

    /** Returns the array index of the current value in its parent, or 0 if not in an array. */
    uint32_t FLDeepIterator_GetIndex(FLDeepIterator) FLAPI;

    /** Returns the current depth in the hierarchy, starting at 1 for the top-level children. */
    size_t FLDeepIterator_GetDepth(FLDeepIterator) FLAPI;

    /** Tells the iterator to skip the children of the current value. */
    void FLDeepIterator_SkipChildren(FLDeepIterator) FLAPI;

    /** Advances the iterator to the next value, or returns false if at the end. */
    bool FLDeepIterator_Next(FLDeepIterator) FLAPI;

    typedef struct {
        FLSlice key;        ///< Dict key, or kFLSliceNull if none
        uint32_t index;     ///< Array index, only if there's no key
    } FLPathComponent;

    /** Returns the path as an array of FLPathComponents. */
    void FLDeepIterator_GetPath(FLDeepIterator,
                                FLPathComponent* FL_NONNULL * FL_NONNULL outPath,
                                size_t* outDepth) FLAPI;

    /** Returns the current path in JavaScript format. */
    FLSliceResult FLDeepIterator_GetPathString(FLDeepIterator) FLAPI;

    /** Returns the current path in JSONPointer format (RFC 6901). */
    FLSliceResult FLDeepIterator_GetJSONPointer(FLDeepIterator) FLAPI;


    //////// PATH


    /** @} */
    /** \defgroup FLKeyPath   Fleece Paths
        @{
     An FLKeyPath Describes a location in a Fleece object tree, as a path from the root that follows
     dictionary properties and array elements.
     It's similar to a JSONPointer or an Objective-C KeyPath, but simpler (so far.)
     The path is compiled into an efficient form that can be traversed quickly.

     It looks like `foo.bar[2][-3].baz` -- that is, properties prefixed with a `.`, and array
     indexes in brackets. (Negative indexes count from the end of the array.)

     A leading JSONPath-like `$.` is allowed but ignored.

     A '\' can be used to escape a special character ('.', '[' or '$') at the start of a
     property name (but not yet in the middle of a name.)
     */

#ifndef FL_IMPL
    typedef struct _FLKeyPath*     FLKeyPath;       ///< A reference to a key path.
#endif

    /** Creates a new FLKeyPath object by compiling a path specifier string. */
    FLKeyPath FL_NULLABLE FLKeyPath_New(FLSlice specifier, FLError* FL_NULLABLE outError) FLAPI;

    /** Frees a compiled FLKeyPath object. (It's ok to pass NULL.) */
    void FLKeyPath_Free(FLKeyPath FL_NULLABLE) FLAPI;

    /** Evaluates a compiled key-path for a given Fleece root object. */
    FLValue FL_NULLABLE FLKeyPath_Eval(FLKeyPath, FLValue root) FLAPI;

    /** Evaluates a key-path from a specifier string, for a given Fleece root object.
        If you only need to evaluate the path once, this is a bit faster than creating an
        FLKeyPath object, evaluating, then freeing it. */
    FLValue FL_NULLABLE FLKeyPath_EvalOnce(FLSlice specifier, FLValue root,
                               FLError* FL_NULLABLE outError) FLAPI;

    /** Returns a path in string form. */
    FLStringResult FLKeyPath_ToString(FLKeyPath path) FLAPI;

    /** Equality test. */
    bool FLKeyPath_Equals(FLKeyPath path1, FLKeyPath path2) FLAPI;

    /** Returns an element of a path, either a key or an array index. */
    bool FLKeyPath_GetElement(FLKeyPath,
                              size_t i,
                              FLSlice *outDictKey,
                              int32_t *outArrayIndex) FLAPI;

    //////// SHARED KEYS


    /** @} */
    /** \defgroup FLSharedKeys   Shared Keys
        @{
        FLSharedKeys represents a mapping from short strings to small integers in the range
        [0...2047]. It's used by FLDict to abbreviate dictionary keys. A shared key can be stored in
        a fixed two bytes and is faster to compare against. However, the same mapping has to be used
        when encoding and when accessing the Dict.

        To use shared keys:
        * Call \ref FLSharedKeys_New to create a new empty mapping.
        * After creating an FLEncoder, call \ref FLEncoder_SetSharedKeys so dictionary keys will
          be added to the mapping and written in integer form.
        * When loading Fleece data, use \ref FLDoc_FromResultData and pass the FLSharedKeys as
          a parameter.
        * Save the mapping somewhere by calling \ref FLSharedKeys_GetStateData or
          \ref FLSharedKeys_WriteState.
        * You can later reconstitute the mapping by calling \ref FLSharedKeys_LoadStateData
          or \ref FLSharedKeys_LoadState on a new empty instance.
        */

    /** Creates a new empty FLSharedKeys object, which must eventually be released. */
    FLSharedKeys FLSharedKeys_New(void) FLAPI;

    typedef bool (*FLSharedKeysReadCallback)(void* FL_NULLABLE context, FLSharedKeys);

    FLSharedKeys FLSharedKeys_NewWithRead(FLSharedKeysReadCallback,
                                          void* FL_NULLABLE context) FLAPI;

    /** Returns a data blob containing the current state (all the keys and their integers.) */
    FLSliceResult FLSharedKeys_GetStateData(FLSharedKeys) FLAPI;

    /** Updates an FLSharedKeys with saved state data created by \ref FLSharedKeys_GetStateData. */
    bool FLSharedKeys_LoadStateData(FLSharedKeys, FLSlice) FLAPI;

    /** Writes the current state to a Fleece encoder as a single value,
        which can later be decoded and passed to \ref FLSharedKeys_LoadState. */
    void FLSharedKeys_WriteState(FLSharedKeys, FLEncoder) FLAPI;

    /** Updates an FLSharedKeys object with saved state, a Fleece value previously written by
        \ref FLSharedKeys_WriteState. */
    bool FLSharedKeys_LoadState(FLSharedKeys, FLValue) FLAPI;

    /** Maps a key string to a number in the range [0...2047], or returns -1 if it isn't mapped.
        If the key doesn't already have a mapping, and the `add` flag is true,
        a new mapping is assigned and returned.
        However, the `add` flag has no effect if the key is unmappable (is longer than 16 bytes
        or contains non-identifier characters), or if all available integers have been assigned. */
    int FLSharedKeys_Encode(FLSharedKeys, FLString, bool add) FLAPI;

    /** Returns the key string that maps to the given integer `key`, else NULL. */
    FLString FLSharedKeys_Decode(FLSharedKeys, int key) FLAPI;

    /** Returns the number of keys in the mapping. This number increases whenever the mapping
        is changed, and never decreases. */
    unsigned FLSharedKeys_Count(FLSharedKeys) FLAPI;

    /** Reverts an FLSharedKeys by "forgetting" any keys added since it had the count `oldCount`. */
    void FLSharedKeys_RevertToCount(FLSharedKeys, unsigned oldCount) FLAPI;

    /** Increments the reference count of an FLSharedKeys. */
    FLSharedKeys FL_NULLABLE FLSharedKeys_Retain(FLSharedKeys FL_NULLABLE) FLAPI;

    /** Decrements the reference count of an FLSharedKeys, freeing it when it reaches zero. */
    void FLSharedKeys_Release(FLSharedKeys FL_NULLABLE) FLAPI;


    typedef struct _FLSharedKeyScope* FLSharedKeyScope;
    FLSharedKeyScope FLSharedKeyScope_WithRange(FLSlice range, FLSharedKeys) FLAPI;
    void FLSharedKeyScope_Free(FLSharedKeyScope FL_NULLABLE);


    //////// ENCODER


    /** @} */
    /** \defgroup FLEncoder   Fleece Encoders
        @{
        An FLEncoder generates encoded Fleece or JSON data. It's sort of a structured output stream,
        with nesting. There are functions for writing every type of scalar value, and for beginning
        and ending collections. To write a collection you begin it, write its values, then end it.
        (Of course a value in a collection can itself be another collection.) When writing a
        dictionary, you have to call writeKey before writing each value.
     */


    /** \name Setup and configuration
         @{ */

    /** Output formats a FLEncoder can generate. */
    typedef enum {
        kFLEncodeFleece,    ///< Fleece encoding
        kFLEncodeJSON,      ///< JSON encoding
        kFLEncodeJSON5      ///< [JSON5](http://json5.org), an extension of JSON with a more readable syntax
    } FLEncoderFormat;


    /** Creates a new encoder, for generating Fleece data. Call FLEncoder_Free when done. */
    FLEncoder FLEncoder_New(void) FLAPI;

    /** Creates a new encoder, allowing some options to be customized.
        @param format  The output format to generate (Fleece, JSON, or JSON5.)
        @param reserveSize  The number of bytes to preallocate for the output. (Default is 256)
        @param uniqueStrings  (Fleece only) If true, string values that appear multiple times will be written
            as a single shared value. This saves space but makes encoding slightly slower.
            You should only turn this off if you know you're going to be writing large numbers
            of non-repeated strings. (Default is true) */
    FLEncoder FLEncoder_NewWithOptions(FLEncoderFormat format,
                                       size_t reserveSize,
                                       bool uniqueStrings) FLAPI;

    /** Creates a new Fleece encoder that writes to a file, not to memory. */
    FLEncoder FLEncoder_NewWritingToFile(FILE*, bool uniqueStrings) FLAPI;

    /** Frees the space used by an encoder. */
    void FLEncoder_Free(FLEncoder FL_NULLABLE) FLAPI;

    /** Tells the encoder to use a shared-keys mapping when encoding dictionary keys. */
    void FLEncoder_SetSharedKeys(FLEncoder, FLSharedKeys FL_NULLABLE) FLAPI;

    /** Associates an arbitrary user-defined value with the encoder. */
    void FLEncoder_SetExtraInfo(FLEncoder, void* FL_NULLABLE info) FLAPI;

    /** Returns the user-defined value associated with the encoder; NULL by default. */
    void* FLEncoder_GetExtraInfo(FLEncoder) FLAPI;


    /** Tells the encoder to logically append to the given Fleece document, rather than making a
        standalone document. Any calls to FLEncoder_WriteValue() where the value points inside the
        base data will write a pointer back to the original value.
        The resulting data returned by FLEncoder_FinishDoc() will *NOT* be standalone; it can only
        be used by first appending it to the base data.
        @param e  The FLEncoder affected.
        @param base  The base document to create an amendment of.
        @param reuseStrings  If true, then writing a string that already exists in the base will
                    just create a pointer back to the original. But the encoder has to scan the
                    base for strings first.
        @param externPointers  If true, pointers into the base will be marked with the `extern`
                    flag. This allows them to be resolved using the `FLResolver_Begin` function,
                    so that when the delta is used the base document can be anywhere in memory,
                    not just immediately preceding the delta document. */
    void FLEncoder_Amend(FLEncoder e, FLSlice base,
                         bool reuseStrings, bool externPointers) FLAPI;

    /** Returns the `base` value passed to FLEncoder_Amend. */
    FLSlice FLEncoder_GetBase(FLEncoder) FLAPI;

    /** Tells the encoder not to write the two-byte Fleece trailer at the end of the data.
        This is only useful for certain special purposes. */
    void FLEncoder_SuppressTrailer(FLEncoder) FLAPI;

    /** Resets the state of an encoder without freeing it. It can then be reused to encode
        another value. */
    void FLEncoder_Reset(FLEncoder) FLAPI;

    /** Returns the number of bytes encoded so far. */
    size_t FLEncoder_BytesWritten(FLEncoder) FLAPI;

    /** Returns the byte offset in the encoded data where the next value will be written.
        (Due to internal buffering, this is not the same as FLEncoder_BytesWritten.) */
    size_t FLEncoder_GetNextWritePos(FLEncoder) FLAPI;

    /** @} */
    /** \name Writing to the encoder
         @{
        @note The functions that write to the encoder do not return error codes, just a 'false'
        result on error. The actual error is attached to the encoder and can be accessed by calling
        FLEncoder_GetError or FLEncoder_End.

        After an error occurs, the encoder will ignore all subsequent writes. */

    /** Writes a `null` value to an encoder. (This is an explicitly-stored null, like the JSON
        `null`, not the "undefined" value represented by a NULL FLValue pointer.) */
    bool FLEncoder_WriteNull(FLEncoder) FLAPI;

    /** Writes an `undefined` value to an encoder. (Its value when read will not be a `NULL`
        pointer, but it can be recognized by `FLValue_GetType` returning `kFLUndefined`.)
        @note The only real use for writing undefined values is to represent "holes" in an array.
        An undefined dictionary value should be written simply by skipping the key and value. */
    bool FLEncoder_WriteUndefined(FLEncoder) FLAPI;

    /** Writes a boolean value (true or false) to an encoder. */
    bool FLEncoder_WriteBool(FLEncoder, bool) FLAPI;

    /** Writes an integer to an encoder. The parameter is typed as `int64_t` but you can pass any
        integral type (signed or unsigned) except for huge `uint64_t`s.
        The number will be written in a compact form that uses only as many bytes as necessary. */
    bool FLEncoder_WriteInt(FLEncoder, int64_t) FLAPI;

    /** Writes an unsigned integer to an encoder.
        @note This function is only really necessary for huge
        64-bit integers greater than or equal to 2^63, which can't be represented as int64_t. */
    bool FLEncoder_WriteUInt(FLEncoder, uint64_t) FLAPI;

    /** Writes a 32-bit floating point number to an encoder.
        @note As an implementation detail, if the number has no fractional part and can be
        represented exactly as an integer, it'll be encoded as an integer to save space. This is
        transparent to the reader, since if it requests the value as a float it'll be returned
        as floating-point. */
    bool FLEncoder_WriteFloat(FLEncoder, float) FLAPI;

    /** Writes a 64-bit floating point number to an encoder.
        @note As an implementation detail, the number may be encoded as a 32-bit float or even
        as an integer, if this can be done without losing precision. For example, 123.0 will be
        written as an integer, and 123.75 as a float.) */
    bool FLEncoder_WriteDouble(FLEncoder, double) FLAPI;

    /** Writes a string to an encoder. The string must be UTF-8-encoded and must not contain any
        zero bytes.
        @warning Do _not_ use this to write a dictionary key; use FLEncoder_WriteKey instead. */
    bool FLEncoder_WriteString(FLEncoder, FLString) FLAPI;

    /** Writes a timestamp to an encoder, as an ISO-8601 date string.
        @note Since neither Fleece nor JSON have a 'Date' type, the encoded string has no
        metadata that distinguishes it as a date. It's just a string.)
        @param encoder  The encoder to write to.
        @param ts  The timestamp (milliseconds since Unix epoch 1-1-1970).
        @param asUTC  If true, date is written in UTC (GMT); if false, with the local timezone.
        @return  True on success, false on error. */
    bool FLEncoder_WriteDateString(FLEncoder encoder, FLTimestamp ts, bool asUTC) FLAPI;

    /** Writes a binary data value (a blob) to an encoder. This can contain absolutely anything
        including null bytes.
        If the encoder is generating JSON, the blob will be written as a base64-encoded string. */
    bool FLEncoder_WriteData(FLEncoder, FLSlice) FLAPI;

    /** Writes raw data directly to the encoded output.
        (This is not the same as FLEncoder_WriteData, which safely encodes a blob.)
        @warning **Do not call this** unless you really know what you're doing ...
        it's quite unsafe, and only used for certain advanced purposes. */
    bool FLEncoder_WriteRaw(FLEncoder, FLSlice) FLAPI;


    /** Begins writing an array value to an encoder. This pushes a new state where each
        subsequent value written becomes an array item, until FLEncoder_EndArray is called.
        @param reserveCount  Number of array elements to reserve space for. If you know the size
            of the array, providing it here speeds up encoding slightly. If you don't know,
            just use zero. */
    bool FLEncoder_BeginArray(FLEncoder, size_t reserveCount) FLAPI;

    /** Ends writing an array value; pops back the previous encoding state. */
    bool FLEncoder_EndArray(FLEncoder) FLAPI;


    /** Begins writing a dictionary value to an encoder. This pushes a new state where each
        subsequent key and value written are added to the dictionary, until FLEncoder_EndDict is
        called.
        Before adding each value, you must call FLEncoder_WriteKey (_not_ FLEncoder_WriteString!),
        to write the dictionary key.
        @param reserveCount  Number of dictionary items to reserve space for. If you know the size
            of the dictionary, providing it here speeds up encoding slightly. If you don't know,
            just use zero. */
    bool FLEncoder_BeginDict(FLEncoder, size_t reserveCount) FLAPI;

    /** Specifies the key for the next value to be written to the current dictionary. */
    bool FLEncoder_WriteKey(FLEncoder, FLString) FLAPI;

    /** Specifies the key for the next value to be written to the current dictionary.
        The key is given as a Value, which must be a string or integer. */
    bool FLEncoder_WriteKeyValue(FLEncoder, FLValue) FLAPI;

    /** Ends writing a dictionary value; pops back the previous encoding state. */
    bool FLEncoder_EndDict(FLEncoder) FLAPI;


    /** Writes a Fleece Value to an Encoder. */
    bool FLEncoder_WriteValue(FLEncoder, FLValue) FLAPI;


    /** Returns an opaque reference to the last complete value written to the encoder, if possible.
        Fails (returning 0) if nothing has been written, or if the value is inline and can't be
        referenced this way -- that only happens with small scalars or empty collections. */
    intptr_t FLEncoder_LastValueWritten(FLEncoder);

    /** Writes another reference (a "pointer") to an already-written value, given a reference previously
        returned from \ref FLEncoder_LastValueWritten. The effect is exactly the same as if you wrote the
        entire value again, except that the size of the encoded data only grows by 4 bytes. */
    void FLEncoder_WriteValueAgain(FLEncoder, intptr_t preWrittenValue);


    /** Returns the data written so far as a standalone Fleece document, whose root is the last
        value written. You can continue writing, and the final output returned by \ref FLEncoder_Finish will
        consist of everything after this point. That second part can be used in the future by loading it
        as an `FLDoc` with the first part as its `extern` reference. */
    FLSliceResult FLEncoder_Snip(FLEncoder);


    /** Parses JSON data and writes the object(s) to the encoder. (This acts as a single write,
        like WriteInt; it's just that the value written is likely to be an entire dictionary of
        array.) */
    bool FLEncoder_ConvertJSON(FLEncoder, FLSlice json) FLAPI;

    /** @} */
    /** \name Finishing up
         @{ */

    /** Finishes encoding the current item, and returns its offset in the output data. */
    size_t FLEncoder_FinishItem(FLEncoder) FLAPI;

    /** Ends encoding; if there has been no error, it returns the encoded Fleece data packaged in
        an FLDoc. (This function does not support JSON encoding.)
        This does not free the FLEncoder; call FLEncoder_Free (or FLEncoder_Reset) next. */
    MUST_USE_RESULT
    FLDoc FL_NULLABLE FLEncoder_FinishDoc(FLEncoder, FLError* FL_NULLABLE outError) FLAPI;

    /** Ends encoding; if there has been no error, it returns the encoded data, else null.
        This does not free the FLEncoder; call FLEncoder_Free (or FLEncoder_Reset) next. */
    MUST_USE_RESULT
    FLSliceResult FLEncoder_Finish(FLEncoder, FLError* FL_NULLABLE outError) FLAPI;

    /** @} */
    /** \name Error handling
         @{ */

    /** Returns the error code of an encoder, or NoError (0) if there's no error. */
    FLError FLEncoder_GetError(FLEncoder) FLAPI;

    /** Returns the error message of an encoder, or NULL if there's no error. */
    const char* FL_NULLABLE FLEncoder_GetErrorMessage(FLEncoder) FLAPI;

    /** @} */
    /** @} */


    //////// JSON DELTA COMPRESSION


    /** \defgroup delta   Fleece Delta Compression
     @{
        These functions implement a fairly-efficient "delta" encoding that encapsulates the changes
        needed to transform one Fleece value into another. The delta is expressed in JSON form.

        A delta can be stored or transmitted
        as an efficient way to produce the second value, when the first is already present. Deltas
        are frequently used in version-control systems and efficient network protocols.
     */

    /** Returns JSON that encodes the changes to turn the value `old` into `nuu`.
        (The format is documented in Fleece.md, but you should treat it as a black box.)
        @param old  A value that's typically the old/original state of some data.
        @param nuu  A value that's typically the new/changed state of the `old` data.
        @return  JSON data representing the changes from `old` to `nuu`, or NULL on
                    (extremely unlikely) failure. */
    FLSliceResult FLCreateJSONDelta(FLValue FL_NULLABLE old,
                                    FLValue FL_NULLABLE nuu) FLAPI;

    /** Writes JSON that describes the changes to turn the value `old` into `nuu`.
        (The format is documented in Fleece.md, but you should treat it as a black box.)
        @param old  A value that's typically the old/original state of some data.
        @param nuu  A value that's typically the new/changed state of the `old` data.
        @param jsonEncoder  An encoder to write the JSON to. Must have been created using
                `FLEncoder_NewWithOptions`, with JSON or JSON5 format.
        @return  True on success, false on (extremely unlikely) failure. */
    bool FLEncodeJSONDelta(FLValue FL_NULLABLE old,
                           FLValue FL_NULLABLE nuu,
                           FLEncoder jsonEncoder) FLAPI;


    /** Applies the JSON data created by `CreateJSONDelta` to the value `old`, which must be equal
        to the `old` value originally passed to `FLCreateJSONDelta`, and returns a Fleece document
        equal to the original `nuu` value.
        @param old  A value that's typically the old/original state of some data. This must be
                    equal to the `old` value used when creating the `jsonDelta`.
        @param jsonDelta  A JSON-encoded delta created by `FLCreateJSONDelta` or `FLEncodeJSONDelta`.
        @param outError  On failure, error information will be stored where this points, if non-null.
        @return  The corresponding `nuu` value, encoded as Fleece, or null if an error occurred. */
    FLSliceResult FLApplyJSONDelta(FLValue FL_NULLABLE old,
                                   FLSlice jsonDelta,
                                   FLError* FL_NULLABLE outError) FLAPI;

    /** Applies the (parsed) JSON data created by `CreateJSONDelta` to the value `old`, which must be
        equal to the `old` value originally passed to `FLCreateJSONDelta`, and writes the corresponding
        `nuu` value to the encoder.
        @param old  A value that's typically the old/original state of some data. This must be
                    equal to the `old` value used when creating the `jsonDelta`.
        @param jsonDelta  A JSON-encoded delta created by `FLCreateJSONDelta` or `FLEncodeJSONDelta`.
        @param encoder  A Fleece encoder to write the decoded `nuu` value to. (JSON encoding is not
                    supported.)
        @return  True on success, false on error; call `FLEncoder_GetError` for details. */
    bool FLEncodeApplyingJSONDelta(FLValue FL_NULLABLE old,
                                   FLSlice jsonDelta,
                                   FLEncoder encoder) FLAPI;


    //////// VALUE SLOTS


    /** @} */
    /** \defgroup Slots   Value Slots
        @{
         An \ref FLSlot is a temporary reference to an element of a mutable Array/Dict;
         its only purpose is to let you store a value into it, using the `FLSlot_...` functions.

         Since there are three ways to store a value into a collection (array set, array append,
         dict set) and nine types of values that can be stored, that makes 27 setter functions.
         For efficiency, these are declared as inlines that call one of three functions to acquire
         a slot, and one of nine functions to store a value into it.

         It's usually more convenient to use the typed functions like \ref FLMutableArray_SetInt,
         but you might drop down to the lower level ones if you're creating an adapter between
         Fleece and a different data model, such as Apple's Foundation classes. */

    /** Returns an \ref FLSlot that refers to the given index of the given array.
        You store a value to it by calling one of the nine `FLSlot_Set...` functions.
        \warning You should immediately store a value into the `FLSlot`. Do not keep it around;
                 any changes to the array invalidate it.*/
    MUST_USE_RESULT
    FLSlot FLMutableArray_Set(FLMutableArray, uint32_t index) FLAPI;

    /** Appends a null value to the array and returns an \ref FLSlot that refers to that position.
        You store a value to it by calling one of the nine `FLSlot_Set...` functions.
        \warning You should immediately store a value into the `FLSlot`. Do not keep it around;
                 any changes to the array invalidate it.*/
    MUST_USE_RESULT
    FLSlot FLMutableArray_Append(FLMutableArray) FLAPI;

    /** Returns an \ref FLSlot that refers to the given key/value pair of the given dictionary.
        You store a value to it by calling one of the nine `FLSlot_Set...` functions.
        \warning You should immediately store a value into the `FLSlot`. Do not keep it around;
                 any changes to the dictionary invalidate it.*/
    MUST_USE_RESULT
    FLSlot FLMutableDict_Set(FLMutableDict, FLString key) FLAPI;


    void FLSlot_SetNull(FLSlot) FLAPI;             ///< Stores a JSON null into a slot.
    void FLSlot_SetBool(FLSlot, bool) FLAPI;       ///< Stores a boolean into a slot.
    void FLSlot_SetInt(FLSlot, int64_t) FLAPI;     ///< Stores an integer into a slot.
    void FLSlot_SetUInt(FLSlot, uint64_t) FLAPI;   ///< Stores an unsigned int into a slot.
    void FLSlot_SetFloat(FLSlot, float) FLAPI;     ///< Stores a `float` into a slot.
    void FLSlot_SetDouble(FLSlot, double) FLAPI;   ///< Stores a `double` into a slot.
    void FLSlot_SetString(FLSlot, FLString) FLAPI; ///< Stores a UTF-8 string into a slot.
    void FLSlot_SetData(FLSlot, FLSlice) FLAPI;    ///< Stores a data blob into a slot.
    void FLSlot_SetValue(FLSlot, FLValue) FLAPI;   ///< Stores an FLValue into a slot.
    
    static inline void FLSlot_SetArray(FLSlot slot, FLArray array) {
        FLSlot_SetValue(slot, (FLValue)array);
    }

    static inline void FLSlot_SetDict(FLSlot slot, FLDict dict) {
        FLSlot_SetValue(slot, (FLValue)dict);
    }


    // implementations of the inline methods declared earlier:

    static inline void FLMutableArray_SetNull(FLMutableArray a, uint32_t index) {
        FLSlot_SetNull(FLMutableArray_Set(a, index));
    }
    static inline void FLMutableArray_SetBool(FLMutableArray a, uint32_t index, bool val) {
        FLSlot_SetBool(FLMutableArray_Set(a, index), val);
    }
    static inline void FLMutableArray_SetInt(FLMutableArray a, uint32_t index, int64_t val) {
        FLSlot_SetInt(FLMutableArray_Set(a, index), val);
    }
    static inline void FLMutableArray_SetUInt(FLMutableArray a, uint32_t index, uint64_t val) {
        FLSlot_SetUInt(FLMutableArray_Set(a, index), val);
    }
    static inline void FLMutableArray_SetFloat(FLMutableArray a, uint32_t index, float val) {
        FLSlot_SetFloat(FLMutableArray_Set(a, index), val);
    }
    static inline void FLMutableArray_SetDouble(FLMutableArray a, uint32_t index, double val) {
        FLSlot_SetDouble(FLMutableArray_Set(a, index), val);
    }
    static inline void FLMutableArray_SetString(FLMutableArray a, uint32_t index, FLString val) {
        FLSlot_SetString(FLMutableArray_Set(a, index), val);
    }
    static inline void FLMutableArray_SetData(FLMutableArray a, uint32_t index, FLSlice val) {
        FLSlot_SetData(FLMutableArray_Set(a, index), val);
    }
    static inline void FLMutableArray_SetValue(FLMutableArray a, uint32_t index, FLValue val) {
        FLSlot_SetValue(FLMutableArray_Set(a, index), val);
    }
    static inline void FLMutableArray_SetArray(FLMutableArray a, uint32_t index, FLArray val) {
        FLSlot_SetValue(FLMutableArray_Set(a, index), (FLValue)val);
    }
    static inline void FLMutableArray_SetDict(FLMutableArray a, uint32_t index, FLDict val) {
        FLSlot_SetValue(FLMutableArray_Set(a, index), (FLValue)val);
    }

    static inline void FLMutableArray_AppendNull(FLMutableArray a) {
        FLSlot_SetNull(FLMutableArray_Append(a));
    }
    static inline void FLMutableArray_AppendBool(FLMutableArray a, bool val) {
        FLSlot_SetBool(FLMutableArray_Append(a), val);
    }
    static inline void FLMutableArray_AppendInt(FLMutableArray a, int64_t val) {
        FLSlot_SetInt(FLMutableArray_Append(a), val);
    }
    static inline void FLMutableArray_AppendUInt(FLMutableArray a, uint64_t val) {
        FLSlot_SetUInt(FLMutableArray_Append(a), val);
    }
    static inline void FLMutableArray_AppendFloat(FLMutableArray a, float val) {
        FLSlot_SetFloat(FLMutableArray_Append(a), val);
    }
    static inline void FLMutableArray_AppendDouble(FLMutableArray a, double val) {
        FLSlot_SetDouble(FLMutableArray_Append(a), val);
    }
    static inline void FLMutableArray_AppendString(FLMutableArray a, FLString val) {
        FLSlot_SetString(FLMutableArray_Append(a), val);
    }
    static inline void FLMutableArray_AppendData(FLMutableArray a, FLSlice val) {
        FLSlot_SetData(FLMutableArray_Append(a), val);
    }
    static inline void FLMutableArray_AppendValue(FLMutableArray a, FLValue val) {
        FLSlot_SetValue(FLMutableArray_Append(a), val);
    }
    static inline void FLMutableArray_AppendArray(FLMutableArray a, FLArray val) {
        FLSlot_SetValue(FLMutableArray_Append(a), (FLValue)val);
    }
    static inline void FLMutableArray_AppendDict(FLMutableArray a, FLDict val) {
        FLSlot_SetValue(FLMutableArray_Append(a), (FLValue)val);
    }

    static inline void FLMutableDict_SetNull(FLMutableDict d, FLString key) {
        FLSlot_SetNull(FLMutableDict_Set(d, key));
    }
    static inline void FLMutableDict_SetBool(FLMutableDict d, FLString key, bool val) {
        FLSlot_SetBool(FLMutableDict_Set(d, key), val);
    }
    static inline void FLMutableDict_SetInt(FLMutableDict d, FLString key, int64_t val) {
        FLSlot_SetInt(FLMutableDict_Set(d, key), val);
    }
    static inline void FLMutableDict_SetUInt(FLMutableDict d, FLString key, uint64_t val) {
        FLSlot_SetUInt(FLMutableDict_Set(d, key), val);
    }
    static inline void FLMutableDict_SetFloat(FLMutableDict d, FLString key, float val) {
        FLSlot_SetFloat(FLMutableDict_Set(d, key), val);
    }
    static inline void FLMutableDict_SetDouble(FLMutableDict d, FLString key, double val) {
        FLSlot_SetDouble(FLMutableDict_Set(d, key), val);
    }
    static inline void FLMutableDict_SetString(FLMutableDict d, FLString key, FLString val) {
        FLSlot_SetString(FLMutableDict_Set(d, key), val);
    }
    static inline void FLMutableDict_SetData(FLMutableDict d, FLString key, FLSlice val) {
        FLSlot_SetData(FLMutableDict_Set(d, key), val);
    }
    static inline void FLMutableDict_SetValue(FLMutableDict d, FLString key, FLValue val) {
        FLSlot_SetValue(FLMutableDict_Set(d, key), val);
    }
    static inline void FLMutableDict_SetArray(FLMutableDict d, FLString key, FLArray val) {
        FLSlot_SetValue(FLMutableDict_Set(d, key), (FLValue)val);
    }
    static inline void FLMutableDict_SetDict(FLMutableDict d, FLString key, FLDict val) {
        FLSlot_SetValue(FLMutableDict_Set(d, key), (FLValue)val);
    }


    /** @} */

#ifdef __cplusplus
}
#endif

FL_ASSUME_NONNULL_END

#ifdef __OBJC__
// When compiling as Objective-C, include CoreFoundation / Objective-C utilities:
#include "Fleece+CoreFoundation.h"
#endif

#endif // _FLEECE_H
