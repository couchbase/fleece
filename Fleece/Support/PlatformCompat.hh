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
	#define LITECORE_UNUSED
    #define __typeof                        decltype

    #define __has_extension(X)              0
    #define __has_feature(F)                0
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

    #ifdef __APPLE__
    #define LITECORE_UNUSED __unused
    #else
    #define LITECORE_UNUSED __attribute__((unused))
    #endif

    #define NOINLINE                        __attribute((noinline))

    #if __has_attribute(always_inline)
        #define ALWAYS_INLINE               __attribute__((always_inline)) inline
    #else
        #define ALWAYS_INLINE               inline
    #endif

    // Note: GCC also has a `nonnull` attribute, but it works differently (not as well)
    #ifdef __clang__
        #define NONNULL                     __attribute__((nonnull))
    #else
        #define NONNULL
    #endif

    #ifndef __printflike
    #define __printflike(fmtarg, firstvararg) __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
    #endif

    #define WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) 0

    #define cbl_strdup strdup
    #define cbl_getcwd getcwd

#endif


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

#ifndef __hot
#   if defined(__OPTIMIZE__)
#       if defined(__e2k__)
#           define __hot __attribute__((__hot__)) __optimize(3)
#       elif defined(__clang__) && !__has_attribute(__hot_) \
        && __has_attribute(__section__) && (defined(__linux__) || defined(__gnu_linux__))
            /* just put frequently used functions in separate section */
#           define __hot __attribute__((__section__("text.hot"))) __optimize("O3")
#       elif defined(__GNUC__) || __has_attribute(__hot__)
#           define __hot __attribute__((__hot__)) __optimize("O3")
#       else
#           define __hot  __optimize("O3")
#       endif
#   else
#       define __hot
#   endif
#endif /* __hot */

#ifndef __cold
#   if defined(__OPTIMIZE__)
#       if defined(__e2k__)
#           define __cold __attribute__((__cold__)) __optimize(1)
#       elif defined(__clang__) && !__has_attribute(cold) \
        && __has_attribute(__section__) && (defined(__linux__) || defined(__gnu_linux__))
            /* just put infrequently used functions in separate section */
#           define __cold __attribute__((__section__("text.unlikely"))) __optimize("Os")
#       elif defined(__GNUC__) || __has_attribute(cold)
#           define __cold __attribute__((__cold__)) __optimize("Os")
#       else
#           define __cold __optimize("Os")
#       endif
#   else
#       define __cold
#   endif
#endif /* __cold */

