//
// PlatformCompat.hh
//
// Copyright (c) 2016 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once
#include "fleece/Base.h"
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

    #include <time.h>

    #define gmtime_r(a, b)                  (gmtime_s(b, a) == 0 ? b : NULL)
    #define localtime_r(a, b)               (localtime_s(b, a) == 0 ? b : NULL)

    #define timegm                          _mkgmtime

    #include <BaseTsd.h>
    typedef SSIZE_T ssize_t;

    #define MAXFLOAT FLT_MAX

    #define __printflike(A, B)

    #define cbl_strdup _strdup
    #define cbl_getcwd _getcwd

    #include <winapifamily.h>

#else

    // Suppresses "unused function" warnings
    #ifdef __APPLE__
    #define LITECORE_UNUSED __unused
    #else
    #define LITECORE_UNUSED __attribute__((unused))
    #endif

    // Disables inlining a function. Use when the space savings are worth more than speed.
    #define NOINLINE                        __attribute((noinline))

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
    #ifndef __printflike
    #define __printflike(fmtarg, firstvararg) __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
    #endif

    #define WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) 0

    // Windows has underscore prefixes before these function names, so define a common name
    #define cbl_strdup strdup
    #define cbl_getcwd getcwd

#endif


// Note: Code below adapted from libmdbx source code.

// Applies a specific optimization level to a function, e.g. __optimize("O3") or __optimize("Os"),
// Has no effect in an unoptimized build.
#ifndef __optimize
#   if defined(__OPTIMIZE__)
#     if defined(__clang__) && !__has_attribute(__optimize__)
#           define __optimize(ops)
#       elif defined(__GNUC__) || __has_attribute(__optimize__)
#           define __optimize(ops) __attribute__((__optimize__(ops)))
#       else
#           define __optimize(ops)
#       endif
#   else
#           define __optimize(ops)
#   endif
#endif /* __optimize */

#if defined(__clang__)
    #define HOTLEVEL "Ofast"
    #define COLDLEVEL "Oz"
#else
    #define HOTLEVEL "O3"
    #define COLDLEVEL "Os"
#endif

// Marks a function as being a hot-spot. Optimizes it for speed and may move it to a common
// code section for hot functions. Has no effect in an unoptimized build.
#ifndef __hot
#   if defined(__OPTIMIZE__)
#       if defined(__clang__) && !__has_attribute(__hot_) \
        && __has_attribute(__section__) && (defined(__linux__) || defined(__gnu_linux__))
            /* just put frequently used functions in separate section */
#           define __hot __attribute__((__section__("text.hot"))) __optimize(HOTLEVEL)
#       elif defined(__GNUC__) || __has_attribute(__hot__)
#           define __hot __attribute__((__hot__)) __optimize(HOTLEVEL)
#       else
#           define __hot  __optimize(HOTLEVEL)
#       endif
#   else
#       define __hot
#   endif
#endif /* __hot */

// Marks a function as being rarely used (e.g. error handling.) Optimizes it for size and
// moves it to a common code section for cold functions. Has no effect in an unoptimized build.
#ifndef __cold
#   if defined(__OPTIMIZE__)
#       if defined(__clang__) && !__has_attribute(cold) \
        && __has_attribute(__section__) && (defined(__linux__) || defined(__gnu_linux__))
            /* just put infrequently used functions in separate section */
#           define __cold __attribute__((__section__("text.unlikely"))) __optimize(COLDLEVEL)
#       elif defined(__GNUC__) || __has_attribute(cold)
#           define __cold __attribute__((__cold__)) __optimize(COLDLEVEL)
#       else
#           define __cold __optimize(COLDLEVEL)
#       endif
#   else
#       define __cold
#   endif
#endif /* __cold */
