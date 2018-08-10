//
// Base.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once


#ifdef __clang__
    #define NONNULL                         __attribute__((nonnull))
#else
    #define NONNULL
#endif

#ifdef _MSC_VER
    #define _usuallyTrue(VAL)               (VAL)
    #define _usuallyFalse(VAL)              (VAL)
#else
    #define _usuallyTrue(VAL)               __builtin_expect(VAL, true)
    #define _usuallyFalse(VAL)              __builtin_expect(VAL, false)
#endif
