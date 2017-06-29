//
//  PlatformCompat.hh
//  Fleece
//
//  Created by Jens Alfke on 9/8/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once


#ifdef _MSC_VER

    #define _usuallyTrue(VAL)               (VAL)
    #define _usuallyFalse(VAL)              (VAL)
    #define NOINLINE                        __declspec(noinline)
	#define LITECORE_UNUSED
    #define __typeof                        decltype

    #define __has_extension(X)              0
    #define __has_feature(F)                0
    #define __func__                        __FUNCTION__

    #define random()                        rand()
    #define srandom(s)                      srand(s)

    #define localtime_r(a, b)               localtime_s(b, a)
    #define timegm                          _mkgmtime

    #include <BaseTsd.h>
    typedef SSIZE_T ssize_t;

    #define MAXFLOAT FLT_MAX

    #define __printflike(A, B) 

    // MSVC doesn't support C99 so it doesn't have variable-length C arrays.
    // WARNING: sizeof() will not work on this array since it's actually declared as a pointer.
    #define StackArray(NAME, TYPE, SIZE)    TYPE* NAME = (TYPE*)_malloca(sizeof(TYPE)*(SIZE))
    #include <winapifamily.h>

#else

    #if defined(__ANDROID__) && !defined(_LIBCPP_VERSION)
    #include <android/api-level.h>
    #include <math.h>
    #if __ANDROID_API__ < 18
    static inline double log2(double n) {
        return ::log(n) / M_LN2;
    }
    #endif
    #endif

    #ifdef __APPLE__
    #define LITECORE_UNUSED __unused
    #else
    #define LITECORE_UNUSED __attribute__((unused))
    #endif

    #define _usuallyTrue(VAL)               __builtin_expect(VAL, true)
    #define _usuallyFalse(VAL)              __builtin_expect(VAL, false)
    #define NOINLINE                        __attribute((noinline))

    #ifndef __printflike
    #define __printflike(fmtarg, firstvararg) __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
    #endif

    #define StackArray(NAME, TYPE, SIZE)    TYPE NAME[(SIZE)]
    #define WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) 0

#endif
