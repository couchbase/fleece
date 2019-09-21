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
#include "fleece/Base.hh"
#include <clocale>
#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#ifdef _MSC_VER

    #define NOINLINE                        __declspec(noinline)
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
    typedef _locale_t cbl_locale_t;

    #define MAXFLOAT FLT_MAX

    #define __printflike(A, B)

    #define cbl_strdup _strdup
    #define cbl_getcwd _getcwd
    #define cbl_strtod_l _strtod_l
    #define cbl_sprintf_l _sprintf_l

    #include <winapifamily.h>

#else

    #ifdef __APPLE__
    #define LITECORE_UNUSED __unused
    #else
    #define LITECORE_UNUSED __attribute__((unused))
    #endif

    #define NOINLINE                        __attribute((noinline))
    
    #ifdef __clang__
        #define NONNULL                     __attribute__((nonnull))
    #else
        #define NONNULL
    #endif

    #ifndef __printflike
    #define __printflike(fmtarg, firstvararg) __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
    #endif

    #define WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) 0

    typedef locale_t cbl_locale_t;
    
    #include <xlocale.h>
    #define cbl_strdup strdup
    #define cbl_getcwd getcwd
    #define cbl_strtod_l strtod_l
    #define cbl_sprintf_l(buf, fmt, locale, ...) sprintf_l(buf, locale, fmt, __VA_ARGS__)

#endif

#ifdef ESP_PLATFORM
    #include "sdkconfig.h"
    #define FL_EMBEDDED 1
    #if CONFIG_OPTIMIZATION_LEVEL_DEBUG
        #ifndef DEBUG
            #define DEBUG 1
            #undef NDEBUG
        #endif
    #endif
#endif

extern "C" cbl_locale_t c_locale(void);