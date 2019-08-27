//
// Base.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once

// https://clang.llvm.org/docs/AttributeReference.html
// https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html

#if defined(__clang__) || defined(__GNUC__)
    #define RETURNS_NONNULL                 __attribute__((returns_nonnull))
    #define MUST_USE_RESULT                 __attribute__((warn_unused_result))

    #define _usuallyTrue(VAL)               __builtin_expect(VAL, true)
    #define _usuallyFalse(VAL)              __builtin_expect(VAL, false)
#else
    #define RETURNS_NONNULL
    #define MUST_USE_RESULT

    #define _usuallyTrue(VAL)               (VAL)
    #define _usuallyFalse(VAL)              (VAL)
#endif


#if defined(__clang__)
    #define NONNULL                         __attribute__((nonnull))
#else
    // GCC does have __attribute__((nonnull)) but it works differently (requires argument numbers)
    #define NONNULL
#endif
