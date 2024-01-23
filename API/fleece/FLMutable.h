//
// FLMutable.h
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
#ifndef _FLMUTABLE_H
#define _FLMUTABLE_H

#include "FLValue.h"

FL_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

    // This is the C API! For the C++ API, see Fleece.hh.


    /** \defgroup Mutable   Mutable Values
        @{ */


    /** Option flags for making mutable copies of values. */
    typedef enum {
        kFLDefaultCopy        = 0,  ///< Shallow copy. References immutables instead of copying.
        kFLDeepCopy           = 1,  ///< Deep copy of mutable values
        kFLCopyImmutables     = 2,  ///< Makes mutable copies of immutables instead of just refs.
        kFLDeepCopyImmutables = (kFLDeepCopy | kFLCopyImmutables), ///< Both deep-copy and copy-immutables.
    } FLCopyFlags;


    //====== MUTABLE ARRAY


     /** \name Mutable Arrays
         @{ */

    /** Creates a new mutable Array that's a copy of the source Array.
        Its initial ref-count is 1, so a call to \ref FLMutableArray_Release will free it.

        Copying an immutable Array is very cheap (only one small allocation) unless the flag
        \ref kFLCopyImmutables is set.

        Copying a mutable Array is cheap if it's a shallow copy; but if \ref kFLDeepCopy is set,
        nested mutable Arrays and Dicts are also copied, recursively; if \ref kFLCopyImmutables is
        also set, immutable values are also copied, recursively.

        If the source Array is NULL, then NULL is returned. */
    NODISCARD FLEECE_PUBLIC FLMutableArray FL_NULLABLE FLArray_MutableCopy(FLArray FL_NULLABLE,
                                                   FLCopyFlags) FLAPI;

    /** Creates a new empty mutable Array.
        Its initial ref-count is 1, so a call to FLMutableArray_Release will free it.  */
    NODISCARD FLEECE_PUBLIC FLMutableArray FL_NULLABLE FLMutableArray_New(void) FLAPI;

    /** Increments the ref-count of a mutable Array. */
    static inline FLMutableArray FL_NULLABLE FLMutableArray_Retain(FLMutableArray FL_NULLABLE d) {
        return (FLMutableArray)FLValue_Retain((FLValue)d);
    }
    /** Decrements the refcount of (and possibly frees) a mutable Array. */
    static inline void FLMutableArray_Release(FLMutableArray FL_NULLABLE d) {
        FLValue_Release((FLValue)d);
    }

    /** If the Array was created by FLArray_MutableCopy, returns the original source Array. */
    FLEECE_PUBLIC FLArray FL_NULLABLE FLMutableArray_GetSource(FLMutableArray FL_NULLABLE) FLAPI;

    /** Returns true if the Array has been changed from the source it was copied from. */
    FLEECE_PUBLIC bool FLMutableArray_IsChanged(FLMutableArray FL_NULLABLE) FLAPI;

    /** Sets or clears the mutable Array's "changed" flag. */
    FLEECE_PUBLIC void FLMutableArray_SetChanged(FLMutableArray FL_NULLABLE,
                                   bool changed) FLAPI;

    /** Inserts a contiguous range of JSON `null` values into the array.
        @param array  The array to operate on.
        @param firstIndex  The zero-based index of the first value to be inserted.
        @param count  The number of items to insert. */
    FLEECE_PUBLIC void FLMutableArray_Insert(FLMutableArray FL_NULLABLE array,
                               uint32_t firstIndex,
                               uint32_t count) FLAPI;

    /** Removes contiguous items from the array.
        @param array  The array to operate on.
        @param firstIndex  The zero-based index of the first item to remove.
        @param count  The number of items to remove. */
    FLEECE_PUBLIC void FLMutableArray_Remove(FLMutableArray FL_NULLABLE array,
                               uint32_t firstIndex,
                               uint32_t count) FLAPI;

    /** Changes the size of an array.
        If the new size is larger, the array is padded with JSON `null` values.
        If it's smaller, values are removed from the end. */
    FLEECE_PUBLIC void FLMutableArray_Resize(FLMutableArray FL_NULLABLE array,
                               uint32_t size) FLAPI;

    /** Convenience function for getting an array-valued property in mutable form.
        - If the value for the key is not an array, returns NULL.
        - If the value is a mutable array, returns it.
        - If the value is an immutable array, this function makes a mutable copy, assigns the
          copy as the property value, and returns the copy. */
    NODISCARD FLEECE_PUBLIC FLMutableArray FL_NULLABLE FLMutableArray_GetMutableArray(FLMutableArray FL_NULLABLE,
                                                              uint32_t index) FLAPI;

    /** Convenience function for getting an array-valued property in mutable form.
        - If the value for the key is not an array, returns NULL.
        - If the value is a mutable array, returns it.
        - If the value is an immutable array, this function makes a mutable copy, assigns the
          copy as the property value, and returns the copy. */
    NODISCARD FLEECE_PUBLIC FLMutableDict FL_NULLABLE FLMutableArray_GetMutableDict(FLMutableArray FL_NULLABLE,
                                                            uint32_t index) FLAPI;


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


    //====== MUTABLE DICT


    /** \name Mutable dictionaries
         @{ */

    /** Creates a new mutable Dict that's a copy of the source Dict.
        Its initial ref-count is 1, so a call to FLMutableDict_Release will free it.

        Copying an immutable Dict is very cheap (only one small allocation.) The `deepCopy` flag
        is ignored.

        Copying a mutable Dict is cheap if it's a shallow copy, but if `deepCopy` is true,
        nested mutable Dicts and Arrays are also copied, recursively.

        If the source dict is NULL, then NULL is returned. */
    FLEECE_PUBLIC FLMutableDict FL_NULLABLE FLDict_MutableCopy(FLDict FL_NULLABLE source, FLCopyFlags) FLAPI;

    /** Creates a new empty mutable Dict.
        Its initial ref-count is 1, so a call to FLMutableDict_Release will free it.  */
    FLEECE_PUBLIC FLMutableDict FL_NULLABLE FLMutableDict_New(void) FLAPI;

    /** Increments the ref-count of a mutable Dict. */
    static inline FLMutableDict FL_NULLABLE FLMutableDict_Retain(FLMutableDict FL_NULLABLE d) {
        return (FLMutableDict)FLValue_Retain((FLValue)d);
    }

    /** Decrements the refcount of (and possibly frees) a mutable Dict. */
    static inline void FLMutableDict_Release(FLMutableDict FL_NULLABLE d) {
        FLValue_Release((FLValue)d);
    }

    /** If the Dict was created by FLDict_MutableCopy, returns the original source Dict. */
    FLEECE_PUBLIC FLDict FL_NULLABLE FLMutableDict_GetSource(FLMutableDict FL_NULLABLE) FLAPI;

    /** Returns true if the Dict has been changed from the source it was copied from. */
    FLEECE_PUBLIC bool FLMutableDict_IsChanged(FLMutableDict FL_NULLABLE) FLAPI;

    /** Sets or clears the mutable Dict's "changed" flag. */
    FLEECE_PUBLIC void FLMutableDict_SetChanged(FLMutableDict FL_NULLABLE, bool) FLAPI;

    /** Removes the value for a key. */
    FLEECE_PUBLIC void FLMutableDict_Remove(FLMutableDict FL_NULLABLE, FLString key) FLAPI;

    /** Removes all keys and values. */
    FLEECE_PUBLIC void FLMutableDict_RemoveAll(FLMutableDict FL_NULLABLE) FLAPI;

    /** Convenience function for getting an array-valued property in mutable form.
        - If the value for the key is not an array, returns NULL.
        - If the value is a mutable array, returns it.
        - If the value is an immutable array, this function makes a mutable copy, assigns the
          copy as the property value, and returns the copy. */
    FLEECE_PUBLIC FLMutableArray FL_NULLABLE FLMutableDict_GetMutableArray(FLMutableDict FL_NULLABLE, FLString key) FLAPI;

    /** Convenience function for getting a dict-valued property in mutable form.
        - If the value for the key is not a dict, returns NULL.
        - If the value is a mutable dict, returns it.
        - If the value is an immutable dict, this function makes a mutable copy, assigns the
          copy as the property value, and returns the copy. */
    FLEECE_PUBLIC FLMutableDict FL_NULLABLE FLMutableDict_GetMutableDict(FLMutableDict FL_NULLABLE, FLString key) FLAPI;


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


    //====== NEWSTRING, NEWDATA


    /** \name Creating string and data values
        @{ */

    /** Allocates a string value on the heap. This is rarely needed -- usually you'd just add a string
     to a mutable Array or Dict directly using one of their "...SetString" or "...AppendString"
     methods. */
    FLEECE_PUBLIC FLValue FL_NULLABLE FLValue_NewString(FLString) FLAPI;

    /** Allocates a data/blob value on the heap. This is rarely needed -- usually you'd just add data
     to a mutable Array or Dict directly using one of their "...SetData or "...AppendData"
     methods. */
    FLEECE_PUBLIC FLValue FL_NULLABLE FLValue_NewData(FLSlice) FLAPI;

    /** @} */


    //====== VALUE SLOTS


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
    NODISCARD
    FLEECE_PUBLIC FLSlot FLMutableArray_Set(FLMutableArray, uint32_t index) FLAPI;

    /** Appends a null value to the array and returns an \ref FLSlot that refers to that position.
        You store a value to it by calling one of the nine `FLSlot_Set...` functions.
        \warning You should immediately store a value into the `FLSlot`. Do not keep it around;
                 any changes to the array invalidate it.*/
    NODISCARD
    FLEECE_PUBLIC FLSlot FLMutableArray_Append(FLMutableArray) FLAPI;

    /** Returns an \ref FLSlot that refers to the given key/value pair of the given dictionary.
        You store a value to it by calling one of the nine `FLSlot_Set...` functions.
        \warning You should immediately store a value into the `FLSlot`. Do not keep it around;
                 any changes to the dictionary invalidate it.*/
    NODISCARD
    FLEECE_PUBLIC FLSlot FLMutableDict_Set(FLMutableDict, FLString key) FLAPI;


    FLEECE_PUBLIC void FLSlot_SetNull(FLSlot) FLAPI;             ///< Stores a JSON null into a slot.
    FLEECE_PUBLIC void FLSlot_SetBool(FLSlot, bool) FLAPI;       ///< Stores a boolean into a slot.
    FLEECE_PUBLIC void FLSlot_SetInt(FLSlot, int64_t) FLAPI;     ///< Stores an integer into a slot.
    FLEECE_PUBLIC void FLSlot_SetUInt(FLSlot, uint64_t) FLAPI;   ///< Stores an unsigned int into a slot.
    FLEECE_PUBLIC void FLSlot_SetFloat(FLSlot, float) FLAPI;     ///< Stores a `float` into a slot.
    FLEECE_PUBLIC void FLSlot_SetDouble(FLSlot, double) FLAPI;   ///< Stores a `double` into a slot.
    FLEECE_PUBLIC void FLSlot_SetString(FLSlot, FLString) FLAPI; ///< Stores a UTF-8 string into a slot.
    FLEECE_PUBLIC void FLSlot_SetData(FLSlot, FLSlice) FLAPI;    ///< Stores a data blob into a slot.
    FLEECE_PUBLIC void FLSlot_SetValue(FLSlot, FLValue) FLAPI;   ///< Stores an FLValue into a slot.

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
    /** @} */

#ifdef __cplusplus
}
#endif

FL_ASSUME_NONNULL_END

#endif // _FLMUTABLE_H
