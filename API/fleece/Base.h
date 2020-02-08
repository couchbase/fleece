//
// Base.h
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once

// https://clang.llvm.org/docs/AttributeReference.html
// https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html

#if defined(__clang__) || defined(__GNUC__)
    // Tells the optimizer that a function's return value is never NULL.
    #define RETURNS_NONNULL                 __attribute__((returns_nonnull))

    // Triggers a compile error if a call to the function ignores the returned value.
    // Typically this is because the return value is something that must be released/freed.
    #define MUST_USE_RESULT                 __attribute__((warn_unused_result))

    // These have no effect on behavior, but they hint to the optimizer which branch of an 'if'
    // statement to make faster.
    #define _usuallyTrue(VAL)               __builtin_expect(VAL, true)
    #define _usuallyFalse(VAL)              __builtin_expect(VAL, false)
#else
    #define RETURNS_NONNULL
    #define MUST_USE_RESULT

    #define _usuallyTrue(VAL)               (VAL)
    #define _usuallyFalse(VAL)              (VAL)
    #ifndef __has_attribute
    #define __has_attribute(X) 0
    #endif
#endif

#ifndef __has_attribute
#define __has_attribute(X) 0
#endif

// Declares that a parameter must not be NULL. The compiler can sometimes detect violations
// of this at compile time, if the parameter value is a literal.
// The Clang Undefined-Behavior Sanitizer will detect all violations at runtime.
#ifdef __clang__
    #define NONNULL                     __attribute__((nonnull))
#else
    // GCC's' `nonnull` works differently (not as well: requires parameter numbers be given)
    #define NONNULL
#endif


// "Many functions have no effects except the return value, and their
//  return value depends only on the parameters and/or global variables.
//  Such a function can be subject to common subexpression elimination
//  and loop optimization just as an arithmetic operator would be.
//  These functions should be declared with the attribute pure." -- GCC manual
#if defined(__GNUC__) || __has_attribute(__pure__)
    #define FLPURE                      __attribute__((__pure__))
#else
    #define FLPURE
#endif
