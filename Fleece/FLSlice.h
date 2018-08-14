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


#ifdef __cplusplus
extern "C" {
#endif


/** A simple reference to a block of memory. Does not imply ownership. */
typedef struct FLSlice {
    const void *buf;
    size_t size;
} FLSlice;

/** Creates a slice pointing to the contents of a C string. */
static inline FLSlice FLStr(const char *str) {
    FLSlice foo = { str, str ? strlen(str) : 0 };
    return foo;
}

// Macro version of FLStr, for use in initializing compile-time constants.
// STR must be a C string literal.
#ifdef _MSC_VER
    #define FLSTR(STR) {("" STR), sizeof(("" STR))-1}
#else
    #define FLSTR(STR) ((FLSlice){("" STR), sizeof(("" STR))-1})
#endif

// A convenient constant denoting a null slice.
#ifdef _MSC_VER
    static const FLSlice kFLSliceNull = { NULL, 0 };
#else
    #define kFLSliceNull ((FLSlice){NULL, 0})
#endif


/** A block of memory returned from an API call. The caller takes ownership, may modify the
    bytes, and must call FLSliceFree when done. */
typedef struct FLSliceResult {
    void *buf;      // note: not const, since caller owns the buffer
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


/** Equality test of two slices. */
bool FLSlice_Equal(FLSlice a, FLSlice b);

/** Lexicographic comparison of two slices; basically like memcmp(), but taking into account
    differences in length. */
int FLSlice_Compare(FLSlice, FLSlice);

/** Frees the memory of a FLSliceResult. */
void FLSliceResult_Free(FLSliceResult);

#define FL_SLICE_DEFINED


#ifdef __cplusplus
}
#endif
