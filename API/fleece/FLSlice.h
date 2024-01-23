//
//  FLSlice.h
//  Fleece
//
//  Created by Jens Alfke on 8/13/18.
//  Copyright 2018-Present Couchbase, Inc.
//
//  Use of this software is governed by the Business Source License included
//  in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
//  in that file, in accordance with the Business Source License, use of this
//  software will be governed by the Apache License, Version 2.0, included in
//  the file licenses/APL2.txt.
//

#pragma once
#ifndef _FLSLICE_H
#define _FLSLICE_H

#include "CompilerSupport.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


#ifdef __cplusplus
    #include <string>
    namespace fleece { struct alloc_slice; }
#endif


FL_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif


/** \defgroup FLSlice    Slices
    @{ */


/** A simple reference to a block of memory. Does not imply ownership.
    (This is equivalent to the C++ class `slice`.) */
typedef struct FLSlice {
    const void* FL_NULLABLE buf;
    size_t size;

#ifdef __cplusplus
    explicit operator bool() const noexcept FLPURE  {return buf != nullptr;}
    explicit operator std::string() const           {return std::string((char*)buf, size);}
#endif
} FLSlice;


/** A heap-allocated block of memory returned from an API call.
    The caller takes ownership, and must call \ref FLSliceResult_Release when done with it.
    \warning The contents of the block must not be modified, since others may be using it.
    \note This is equivalent to the C++ class `alloc_slice`. In C++ the easiest way to deal with
        a `FLSliceResult` return value is to construct an `alloc_slice` from it, which will
        adopt the reference, and release it in its destructor. For example:
        `alloc_slice foo( CopyFoo() );` */
struct NODISCARD FLSliceResult {
    const void* FL_NULLABLE buf;
    size_t size;

#ifdef __cplusplus
    explicit operator bool() const noexcept FLPURE  {return buf != nullptr;}
    explicit operator FLSlice () const              {return {buf, size};}
    inline explicit operator std::string() const;
#endif
};
typedef struct FLSliceResult FLSliceResult;


/** A heap-allocated, reference-counted slice. This type is really just a hint in an API
    that the data can be retained instead of copied, by assigning it to an alloc_slice.
    You can just treat it like FLSlice. */
#ifdef __cplusplus
    struct FLHeapSlice : public FLSlice {
        constexpr FLHeapSlice() noexcept                           :FLSlice{nullptr, 0} { }
    private:
        constexpr FLHeapSlice(const void *FL_NULLABLE b, size_t s) noexcept    :FLSlice{b, s} { }
        friend struct fleece::alloc_slice;
    };
#else
    typedef FLSlice FLHeapSlice;
#endif


// Aliases used to indicate that a slice is expected to contain UTF-8 data.
typedef FLSlice FLString;
typedef FLSliceResult FLStringResult;


/** A convenient constant denoting a null slice. */
#ifdef _MSC_VER
    static const FLSlice kFLSliceNull = { NULL, 0 };
#else
    #define kFLSliceNull ((FLSlice){NULL, 0})
#endif


/** Exactly like memcmp, but safely handles the case where a or b is NULL and size is 0 (by returning 0),
    instead of producing "undefined behavior" as per the C spec. */
static inline FLPURE int FLMemCmp(const void * FL_NULLABLE a,
                                  const void * FL_NULLABLE b, size_t size) FLAPI
{
    if (_usuallyFalse(size == 0))
        return 0;
    return memcmp(a, b, size);
}

/** Exactly like memcmp, but safely handles the case where dst or src is NULL and size is 0 (as a no-op),
    instead of producing "undefined behavior" as per the C spec. */
static inline void FLMemCpy(void* FL_NULLABLE dst, const void* FL_NULLABLE src, size_t size) FLAPI {
    if (_usuallyTrue(size > 0))
        memcpy(dst, src, size);
}


/** Returns a slice pointing to the contents of a C string.
    It's OK to pass NULL; this returns an empty slice.
    \note If the string is a literal, it's more efficient to use \ref FLSTR instead.
    \note Performance is O(n) with the length of the string, since it has to call `strlen`. */
static inline FLSlice FLStr(const char* FL_NULLABLE str) FLAPI {
    FLSlice foo = { str, str ? strlen(str) : 0 };
    return foo;
}

/// Macro version of \ref FLStr, for use in initializing compile-time constants.
/// `STR` must be a C string literal. Has zero runtime overhead.
#ifdef __cplusplus
    #define FLSTR(STR) (FLSlice {("" STR), sizeof(("" STR))-1})
#else
    #define FLSTR(STR) ((FLSlice){("" STR), sizeof(("" STR))-1})
#endif


/** Equality test of two slices. */
FLEECE_PUBLIC bool FLSlice_Equal(FLSlice a, FLSlice b) FLAPI FLPURE;

/** Lexicographic comparison of two slices; basically like memcmp(), but taking into account
    differences in length. */
FLEECE_PUBLIC int FLSlice_Compare(FLSlice, FLSlice) FLAPI FLPURE;

/** Computes a 32-bit hash of a slice's data, suitable for use in hash tables. */
FLEECE_PUBLIC uint32_t FLSlice_Hash(FLSlice s) FLAPI FLPURE;

/** Copies a slice to a buffer, adding a trailing zero byte to make it a valid C string.
    If there is not enough capacity the slice will be truncated, but the trailing zero byte is
    always written.
    @param s  The FLSlice to copy.
    @param buffer  Where to copy the bytes. At least `capacity` bytes must be available.
    @param capacity  The maximum number of bytes to copy (including the trailing 0.)
    @return  True if the entire slice was copied, false if it was truncated. */
FLEECE_PUBLIC bool FLSlice_ToCString(FLSlice s, char* buffer, size_t capacity) FLAPI;

/** Allocates an FLSliceResult of the given size, without initializing the buffer. */
FLEECE_PUBLIC FLSliceResult FLSliceResult_New(size_t) FLAPI;

/** Allocates an FLSliceResult, copying the given slice. */
FLEECE_PUBLIC FLSliceResult FLSlice_Copy(FLSlice) FLAPI;


/** Allocates an FLSliceResult, copying `size` bytes starting at `buf`. */
static inline FLSliceResult FLSliceResult_CreateWith(const void* FL_NULLABLE bytes, size_t size) FLAPI {
    FLSlice s = {bytes, size};
    return FLSlice_Copy(s);
}


FLEECE_PUBLIC void _FLBuf_Retain(const void* FL_NULLABLE) FLAPI;   // internal; do not call
FLEECE_PUBLIC void _FLBuf_Release(const void* FL_NULLABLE) FLAPI;  // internal; do not call

/** Increments the ref-count of a FLSliceResult. */
static inline FLSliceResult FLSliceResult_Retain(FLSliceResult s) FLAPI {
    _FLBuf_Retain(s.buf);
    return s;
}

/** Decrements the ref-count of a FLSliceResult, freeing its memory if it reached zero. */
static inline void FLSliceResult_Release(FLSliceResult s) FLAPI {
    _FLBuf_Release(s.buf);
}

/** Type-casts a FLSliceResult to FLSlice, since C doesn't know it's a subclass. */
static inline FLSlice FLSliceResult_AsSlice(FLSliceResult sr) {
    FLSlice ret;
    memcpy(&ret, &sr, sizeof(ret));
    return ret;
}


/** Writes zeroes to `size` bytes of memory starting at `dst`.
    Unlike a call to `memset`, these writes cannot be optimized away by the compiler.
    This is useful for securely removing traces of passwords or encryption keys. */
FLEECE_PUBLIC void FL_WipeMemory(void *dst, size_t size) FLAPI;


/** @} */

#ifdef __cplusplus
}

    FLPURE static inline bool operator== (FLSlice s1, FLSlice s2) {return FLSlice_Equal(s1, s2);}
    FLPURE static inline bool operator!= (FLSlice s1, FLSlice s2) {return !(s1 == s2);}

    FLPURE static inline bool operator== (FLSliceResult sr, FLSlice s) {return (FLSlice)sr == s;}
    FLPURE static inline bool operator!= (FLSliceResult sr, FLSlice s) {return !(sr ==s);}


    FLSliceResult::operator std::string () const {
        auto str = std::string((char*)buf, size);
        FLSliceResult_Release(*this);
        return str;
    }
#endif

FL_ASSUME_NONNULL_END
#endif // _FLSLICE_H
