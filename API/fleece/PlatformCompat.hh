//
// PlatformCompat.hh
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
#include "fleece/CompilerSupport.h"
#ifdef __APPLE__
    #include <sys/cdefs.h>
    #include "TargetConditionals.h"
#endif

#ifdef _MSC_VER
    #define NOINLINE                        __declspec(noinline)
    #define ALWAYS_INLINE                   inline
    #define ASSUME(cond)                    __assume(cond)
	#define LITECORE_UNUSED
    #define __typeof                        decltype

    #define __func__                        __FUNCTION__

    #include <BaseTsd.h>
    typedef SSIZE_T ssize_t;

    #define MAXFLOAT FLT_MAX

    #define __printflike(A, B)

    #define cbl_strdup _strdup
    #define cbl_getcwd _getcwd

    #include <winapifamily.h>

#else

    // Suppresses "unused function" warnings
    #if __has_attribute(unused)
    #  define LITECORE_UNUSED __attribute__((unused))
    #endif

    // Disables inlining a function. Use when the space savings are worth more than speed.
    #if __has_attribute(noinline)
    #  define NOINLINE                      __attribute((noinline))
    #else
    #  define NOINLINE
    #endif

    // Forces function to be inlined. Use with care for speed-critical code.
    #if __has_attribute(always_inline)
        #define ALWAYS_INLINE               __attribute__((always_inline)) inline
    #else
        #define ALWAYS_INLINE               inline
    #endif

    // Tells the optimizer it may assume `cond` is true (but does not generate code to evaluate it.)
    // A typical use cases is like `ASSUME(x != nullptr)`.
    // Note: Avoid putting function calls inside it; I've seen cases where those functions appear
    // inlined at the call site in the optimized code, even though they're not supposed to.)
    #if __has_builtin(__builtin_assume)
        #define ASSUME(cond)                __builtin_assume(cond)
    #else
        #define ASSUME(cond)                (void(0))
    #endif

    // Declares this function takes a printf-like format string, and the subsequent args should
    // be type-checked against it.
    #if __has_attribute(__format__) && !defined(__printflike)
    #  define __printflike(fmtarg, firstvararg) \
                            __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
    #endif

    // Windows has underscore prefixes before these function names, so define a common name
    #define cbl_strdup strdup
    #define cbl_getcwd getcwd

#endif

// Platform independent string substitutions
#if defined(__linux__)
#define PRIms "ld"
#else
#define PRIms "lld"
#endif
