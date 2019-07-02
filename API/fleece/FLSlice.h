//
//  FLSlice.h
//  Fleece
//
//  Created by Jens Alfke on 8/13/18.
//  Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


// Export/import stuff:  see <https://gcc.gnu.org/wiki/Visibility>
#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef FLEECE_EXPORTS
        #define FLPUBLIC __declspec(dllexport)
    #else
        #define FLPUBLIC __declspec(dllimport)
    #endif
#else
    #ifdef FLEECE_EXPORTS
        #define FLPUBLIC  __attribute__ ((visibility ("default")))
    #else
        #define FLPUBLIC
    #endif
#endif


#ifdef __cplusplus
extern "C" {
#endif


/** \defgroup FLSlice    Slices
    @{ */


/** A simple reference to a block of memory. Does not imply ownership.
    (This is equivalent to the C++ class `slice`.) */
typedef struct FLSlice {
    const void *buf;
    size_t size;
} FLSlice;


/** A block of memory returned from an API call. The caller takes ownership, and must call
    FLSlice_Release (or FLSlice_Free) when done. The heap block may be shared with other users,
    so it must not be modified.
    (This is equivalent to the C++ class `alloc_slice`.) */
typedef struct FLSliceResult {
    const void *buf;
    size_t size;

#ifdef __cplusplus
    explicit operator FLSlice () const {return {buf, size};}
#endif
} FLSliceResult;


/** A heap-allocated, reference-counted slice. This type is really just a hint in an API
    that the data can be retained instead of copied, by assigning it to an alloc_slice.
    You can just treat it like FLSlice. */
#ifdef __cplusplus
    struct FLHeapSlice : public FLSlice {
        FLHeapSlice()                           {buf = nullptr; size = 0;}
        FLHeapSlice(const void *b, size_t s)    {buf = b; size = s;}
    };
#else
    typedef FLSlice FLHeapSlice;
#endif


typedef FLSlice FLString;
typedef FLSliceResult FLStringResult;


/** A convenient constant denoting a null slice. */
#ifdef _MSC_VER
    static const FLSlice kFLSliceNull = { NULL, 0 };
#else
    #define kFLSliceNull ((FLSlice){NULL, 0})
#endif


/** Returns a slice pointing to the contents of a C string.
    (Performance is O(n) with the length of the string, since it has to call `strlen`.) */
static inline FLSlice FLStr(const char *str) {
    FLSlice foo = { str, str ? strlen(str) : 0 };
    return foo;
}

// Macro version of FLStr, for use in initializing compile-time constants.
// STR must be a C string literal. Has zero runtime overhead.
#ifdef _MSC_VER
    #define FLSTR(STR) {("" STR), sizeof(("" STR))-1}
#else
    #define FLSTR(STR) ((FLSlice){("" STR), sizeof(("" STR))-1})
#endif


/** Equality test of two slices. */
FLPUBLIC bool FLSlice_Equal(FLSlice a, FLSlice b);

/** Lexicographic comparison of two slices; basically like memcmp(), but taking into account
    differences in length. */
FLPUBLIC int FLSlice_Compare(FLSlice, FLSlice);

/** Increments the ref-count of a FLSliceResult. */
FLPUBLIC FLSliceResult FLSliceResult_Retain(FLSliceResult);

/** Decrements the ref-count of a FLSliceResult, freeing its memory if it reached zero. */
FLPUBLIC void FLSliceResult_Release(FLSliceResult);

/** Frees a FLSliceResult. (Actually it decrements its ref-count, only freeing the memory it
    points to when the ref-count reaches zero.)
    (This is identical to FLSliceResult_Release, but kept for compatibility reasons.) */
static inline void FLSliceResult_Free(FLSliceResult s) {
    FLSliceResult_Release(s);
}

/** Allocates an FLSliceResult, copying the given slice. */
FLPUBLIC FLSliceResult FLSlice_Copy(FLSlice);


/** @} */

#ifdef __cplusplus
}
#endif
