//
// FLCollections.h
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
#ifndef _FLCOLLECTIONS_H
#define _FLCOLLECTIONS_H

#include "FLBase.h"

FL_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

    // This is the C API! For the C++ API, see Fleece.hh.

    //====== ARRAY


    /** \defgroup FLArray   Fleece Arrays
        @{
        FLArray is a "subclass" of FLValue, representing values that are arrays. It's always OK to
        pass an FLArray to a function parameter expecting an FLValue, even though the compiler
        makes you use an explicit type-cast. It's safe to type-cast the other direction, from
        FLValue to FLArray, _only_ if you already know that the value is an array, e.g. by having
        called FLValue_GetType on it. But it's safer to call FLValue_AsArray instead, since it
        will return NULL if the value isn't an array.
     */

    /** A constant empty array value. */
    FLEECE_PUBLIC extern const FLArray kFLEmptyArray;

    /** Returns the number of items in an array, or 0 if the pointer is NULL. */
    FLEECE_PUBLIC uint32_t FLArray_Count(FLArray FL_NULLABLE) FLAPI FLPURE;

    /** Returns true if an array is empty (or NULL). Depending on the array's representation,
        this can be faster than `FLArray_Count(a) == 0` */
    FLEECE_PUBLIC bool FLArray_IsEmpty(FLArray FL_NULLABLE) FLAPI FLPURE;

    /** If the array is mutable, returns it cast to FLMutableArray, else NULL. */
    NODISCARD FLEECE_PUBLIC FLMutableArray FL_NULLABLE FLArray_AsMutable(FLArray FL_NULLABLE) FLAPI FLPURE;

    /** Returns an value at an array index, or NULL if the index is out of range. */
    FLEECE_PUBLIC FLValue FL_NULLABLE FLArray_Get(FLArray FL_NULLABLE, uint32_t index) FLAPI FLPURE;

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
        Call FLArrayIteratorGetValue to get the first item, then as long as the item is not NULL,
        call FLArrayIterator_Next to advance. */
    FLEECE_PUBLIC void FLArrayIterator_Begin(FLArray FL_NULLABLE, FLArrayIterator*) FLAPI;

    /** Returns the current value being iterated over, or NULL at the end. */
    FLEECE_PUBLIC FLValue FL_NULLABLE FLArrayIterator_GetValue(const FLArrayIterator*) FLAPI FLPURE;

    /** Returns a value in the array at the given offset from the current value. */
    FLEECE_PUBLIC FLValue FL_NULLABLE FLArrayIterator_GetValueAt(const FLArrayIterator*, uint32_t offset) FLAPI FLPURE;

    /** Returns the number of items remaining to be iterated, including the current one. */
    FLEECE_PUBLIC uint32_t FLArrayIterator_GetCount(const FLArrayIterator*) FLAPI FLPURE;

    /** Advances the iterator to the next value.
        @warning It is illegal to call this when the iterator is already at the end.
                 In particular, calling this when the array is empty is always illegal. */
    FLEECE_PUBLIC bool FLArrayIterator_Next(FLArrayIterator*) FLAPI;

    /** @} */
    /** @} */


    //====== DICT


    /** \defgroup FLDict   Fleece Dictionaries
        @{ */

    /** A constant empty array value. */
    FLEECE_PUBLIC extern const FLDict kFLEmptyDict;

    /** Returns the number of items in a dictionary, or 0 if the pointer is NULL. */
    FLEECE_PUBLIC uint32_t FLDict_Count(FLDict FL_NULLABLE) FLAPI FLPURE;

    /** Returns true if a dictionary is empty (or NULL). Depending on the dictionary's
        representation, this can be faster than `FLDict_Count(a) == 0` */
    FLEECE_PUBLIC bool FLDict_IsEmpty(FLDict FL_NULLABLE) FLAPI FLPURE;

    /** If the dictionary is mutable, returns it cast to FLMutableDict, else NULL. */
    FLEECE_PUBLIC FLMutableDict FL_NULLABLE FLDict_AsMutable(FLDict FL_NULLABLE) FLAPI FLPURE;

    /** Looks up a key in a dictionary, returning its value.
        Returns NULL if the value is not found or if the dictionary is NULL. */
    FLEECE_PUBLIC FLValue FL_NULLABLE FLDict_Get(FLDict FL_NULLABLE, FLSlice keyString) FLAPI FLPURE;


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
        then as long as the item is not NULL, call FLDictIterator_Next to advance. */
    FLEECE_PUBLIC void FLDictIterator_Begin(FLDict FL_NULLABLE, FLDictIterator*) FLAPI;

    /** Returns the current key being iterated over. 
        This Value will be a string or an integer, or NULL when there are no more keys. */
    FLEECE_PUBLIC FLValue FL_NULLABLE FLDictIterator_GetKey(const FLDictIterator*) FLAPI FLPURE;

    /** Returns the current key's string value, or NULL when there are no more keys. */
    FLEECE_PUBLIC FLString FLDictIterator_GetKeyString(const FLDictIterator*) FLAPI;

    /** Returns the current value being iterated over. 
        Returns NULL when there are no more values. */
    FLEECE_PUBLIC FLValue FL_NULLABLE FLDictIterator_GetValue(const FLDictIterator*) FLAPI FLPURE;

    /** Returns the number of items remaining to be iterated, including the current one. */
    FLEECE_PUBLIC uint32_t FLDictIterator_GetCount(const FLDictIterator*) FLAPI FLPURE;

    /** Advances the iterator to the next value.
        @warning It is illegal to call this when the iterator is already at the end.
                 In particular, calling this when the dict is empty is always illegal. */
    FLEECE_PUBLIC bool FLDictIterator_Next(FLDictIterator*) FLAPI;

    /** Cleans up after an iterator. Only needed if (a) the dictionary is a delta, and
        (b) you stop iterating before the end (i.e. before FLDictIterator_Next returns false.) */
    FLEECE_PUBLIC void FLDictIterator_End(FLDictIterator*) FLAPI;


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
    FLEECE_PUBLIC FLDictKey FLDictKey_Init(FLSlice string) FLAPI;

    /** Returns the string value of the key (which it was initialized with.) */
    FLEECE_PUBLIC FLString FLDictKey_GetString(const FLDictKey*) FLAPI;

    /** Looks up a key in a dictionary using an FLDictKey. If the key is found, "hint" data will
        be stored inside the FLDictKey that will speed up subsequent lookups. */
    FLEECE_PUBLIC FLValue FL_NULLABLE FLDict_GetWithKey(FLDict FL_NULLABLE, FLDictKey*) FLAPI;

    
    /** @} */
    /** @} */

#ifdef __cplusplus
}
#endif

FL_ASSUME_NONNULL_END

#endif // _FLCOLLECTIONS_H
