//
// Base.h
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


#ifndef PURE
    /* Many functions have no effects except the return value and their
     * return value depends only on the parameters and/or global variables.
     * Such a function can be subject to common subexpression elimination
     * and loop optimization just as an arithmetic operator would be.
     * These functions should be declared with the attribute pure. */
#   if defined(__GNUC__) || __has_attribute(__pure__)
#       define PURE __attribute__((__pure__))
#   else
#       define PURE
#   endif
#endif /* PURE */

