//
// CompilerSupport.h
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#ifndef _FLEECE_COMPILER_SUPPORT_H
#define _FLEECE_COMPILER_SUPPORT_H

// The __has_xxx() macros are supported by [at least] Clang and GCC.
// Define them to return 0 on other compilers.
// https://clang.llvm.org/docs/AttributeReference.html
// https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html

#ifndef __has_attribute
    #define __has_attribute(x) 0
#endif

#ifndef __has_builtin
    #define __has_builtin(x) 0
#endif

#ifndef __has_feature
    #define __has_feature(x) 0
#endif

#ifndef __has_extension
    #define __has_extension(x) 0
#endif


// Tells the optimizer that a function's return value is never NULL.
#if __has_attribute(returns_nonnull)
#  define RETURNS_NONNULL               __attribute__((returns_nonnull))
#else
#  define RETURNS_NONNULL
#endif

// deprecated; use NODISCARD instead
#if __has_attribute(returns_nonnull)
#  define MUST_USE_RESULT               __attribute__((warn_unused_result))
#else
#  define MUST_USE_RESULT
#endif

// NODISCARD expands to the C++17/C23 `[[nodiscard]]` attribute, or else MUST_USE_RESULT.
// (We can't just redefine MUST_USE_RESULT as `[[nodiscard]]` unfortunately, because the former is
// already in use in positions where `[[nodiscard]]` isn't valid, like at the end of a declaration.)
#if (__cplusplus >= 201700L) || (__STDC_VERSION__ >= 202000)
#  define NODISCARD                     [[nodiscard]]
#else
#  define NODISCARD                     MUST_USE_RESULT
#endif

// These have no effect on behavior, but they hint to the optimizer which branch of an 'if'
// statement to make faster.
#if __has_builtin(__builtin_expect)
#define _usuallyTrue(VAL)               __builtin_expect(VAL, true)
#define _usuallyFalse(VAL)              __builtin_expect(VAL, false)
#else
#define _usuallyTrue(VAL)               (VAL)
#define _usuallyFalse(VAL)              (VAL)
#endif


// Nullability annotations, for function parameters and struct fields.
// In between FL_ASSUME_NONNULL_BEGIN and FL_ASSUME_NONNULL_END, all pointer declarations implicitly
// disallow NULL values, unless annotated with FL_NULLABLE (which must come after the `*`.)
// (FL_NONNULL is occasionally necessary when there are multiple levels of pointers.)
// NOTE: Only supported in Clang, so far.
#if __has_feature(nullability)
#  define FL_ASSUME_NONNULL_BEGIN _Pragma("clang assume_nonnull begin")
#  define FL_ASSUME_NONNULL_END _Pragma("clang assume_nonnull end")
#  define FL_NULLABLE _Nullable
#  define FL_NONNULL _Nonnull
#  define FL_RETURNS_NONNULL __attribute__((returns_nonnull))
#else
#  define FL_ASSUME_NONNULL_BEGIN
#  define FL_ASSUME_NONNULL_END
#  define FL_NULLABLE
#  define FL_NONNULL
#  define FL_RETURNS_NONNULL
#endif


// Declares that a parameter must not be NULL. The compiler can sometimes detect violations
// of this at compile time, if the parameter value is a literal.
// The Clang Undefined-Behavior Sanitizer will detect all violations at runtime.
// GCC also has an attribute with this name, but it's incompatible: it can't be applied to a
// parameter, it has to come after the function and list parameters by number. Oh well.
// TODO: Replace this with the better nullability annotations above.
#if __has_attribute(nonnull)
#  define NONNULL                       __attribute__((nonnull))
#else
#  define NONNULL
#endif


// FLPURE functions are _read-only_. They cannot write to memory (in a way that's detectable),
// and they cannot access volatile data or do I/O.
//
// Calling an FLPURE function twice in a row with the same arguments must return the same result.
//
// "Many functions have no effects except the return value, and their return value depends only on
//  the parameters and/or global variables. Such a function can be subject to common subexpression
//  elimination and loop optimization just as an arithmetic operator would be. These functions
//  should be declared with the attribute pure."
// "The pure attribute prohibits a function from modifying the state of the program that is
//  observable by means other than inspecting the function’s return value. However, functions
//  declared with the pure attribute can safely read any non-volatile objects, and modify the value
//  of objects in a way that does not affect their return value or the observable state of the
//  program." -- GCC manual
#if __has_attribute(__pure__)
#  define FLPURE                        __attribute__((__pure__))
#else
#  define FLPURE
#endif

// FLCONST is even stricter than FLPURE. The function cannot access memory at all (except for
// reading immutable values like constants.) The return value can only depend on the parameters.
//
// Calling an FLCONST function with the same arguments must _always_ return the same result.
//
// "Calls to functions whose return value is not affected by changes to the observable state of the
//  program and that have no observable effects on such state other than to return a value may lend
//  themselves to optimizations such as common subexpression elimination. Declaring such functions
//  with the const attribute allows GCC to avoid emitting some calls in repeated invocations of the
//  function with the same argument values."
// "Note that a function that has pointer arguments and examines the data pointed to must not be
//  declared const if the pointed-to data might change between successive invocations of the
//  function.
// "In general, since a function cannot distinguish data that might change from data that cannot,
//  const functions should never take pointer or, in C++, reference arguments. Likewise, a function
//  that calls a non-const function usually must not be const itself." -- GCC manual
#if __has_attribute(__const__)
#  define FLCONST                     __attribute__((__const__))
#else
#  define FLCONST
#endif


// `constexpr14` is for uses of `constexpr` that are valid in C++14 but not earlier.
// In constexpr functions this includes `if`, `for`, `while` statements; or multiple `return`s.
// The macro expands to `constexpr` in C++14 or later, otherwise to nothing.
#ifdef __cplusplus
    #if __cplusplus >= 201400L || _MSVC_LANG >= 201400L
        #define constexpr14 constexpr
    #else
        #define constexpr14
    #endif
#endif // __cplusplus


// STEPOVER is for trivial little glue functions that are annoying to step into in the debugger
// on the way to the function you _do_ want to step into. Examples are RefCounted's operator->,
// or slice constructors. Suppressing debug info for those functions means the debugger
// will continue through them when stepping in.
// (It probably also makes the debug-symbol file smaller.)
#if __has_attribute(nodebug)
    #define STEPOVER __attribute((nodebug))
#else
    #define STEPOVER
#endif


// Note: Code below adapted from libmdbx source code.

// `__optimize` is used by the macros below -- you should probably not use it directly, instead
// use `__hot` or `__cold`. It applies a specific compiler optimization level to a function,
// e.g. __optimize("O3") or __optimize("Os"). Has no effect in an unoptimized build.
#ifndef __optimize
#   if defined(__OPTIMIZE__)
#       if defined(__clang__) && !__has_attribute(__optimize__)
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

// `__hot` marks a function as being a hot-spot. Optimizes it for speed and may move it to a common
// code section for hot functions. Has no effect in an unoptimized build.
#ifndef __hot
#   if defined(__OPTIMIZE__)
#       if defined(__clang__) && !__has_attribute(__hot__) \
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

// `__cold` marks a function as being rarely used (e.g. error handling.) Optimizes it for size and
// moves it to a common code section for cold functions. Has no effect in an unoptimized build.
#ifndef __cold
#   if defined(__OPTIMIZE__)
#       if defined(__clang__) && !__has_attribute(__cold__) \
        && __has_attribute(__section__) && (defined(__linux__) || defined(__gnu_linux__))
            /* just put infrequently used functions in separate section */
#           define __cold __attribute__((__section__("text.unlikely"))) __optimize(COLDLEVEL)
#       elif defined(__GNUC__) || __has_attribute(__cold__)
#           define __cold __attribute__((__cold__)) __optimize(COLDLEVEL)
#       else
#           define __cold __optimize(COLDLEVEL)
#       endif
#   else
#       define __cold
#   endif
#endif /* __cold */


#ifndef _MSC_VER
    #define WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) 0
#endif

// On Windows, FLEECE_PUBLIC marks symbols as being exported from the shared library.
// However, this is not the whole list of things that are exported.  The API methods
// are exported using a definition list, but it is not possible to correctly include
// initialized global variables, so those need to be marked (both in the header and
// implementation) with FLEECE_PUBLIC.  See kFLNullValue below and in Fleece.cc
// for an example.
#if defined(_MSC_VER)
#ifdef FLEECE_EXPORTS
#define FLEECE_PUBLIC __declspec(dllexport)
#else
#define FLEECE_PUBLIC __declspec(dllimport)
#endif
#else
#define FLEECE_PUBLIC __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
    #define FLAPI noexcept
#else
    #define FLAPI
#endif

#else // _FLEECE_COMPILER_SUPPORT_H
#warn "Compiler is not honoring #pragma once"
#endif
