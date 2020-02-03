//
// betterassert.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

// This is an alternate implementation of assert() that produces a nicer message that includes
// the function name, and throws a C++ exception instead of calling abort().
// NOTE: If <assert.h> is included later than this header, it will overwrite this definition of
// assert() with the usual one. (And vice versa.)

// `assert_always()`, `precondition()`, and `postcondition()` do basically the same thing:
// if the boolean parameter is false, they log a message (to stderr) and throw an exception.
// They just throw different exceptions with different messages.
//
// * `precondition()` should be used at the start of a function/method to test its parameters
//   or initial state.
//   It throws `std::invalid_argument`. A failure is interpreted as a bug in the method's _caller_.
// * `postcondition()` should be used at the end of a function/method to test its return value
//   or final state.
//   It throws `fleece::assertion_failure`. A failure is interpreted as a bug in the _method_.
// * `assert_always()` can be used in between to test intermediate state or results.
//   It throws `fleece::assertion_failure`.
//   A failure may be a bug in the method, or in something it called.
//
// These are enabled in all builds regardless of the `NDEBUG` flag.

#ifndef assert_always
    #include "PlatformCompat.hh"
    #include <stdexcept>

    #ifdef _MSC_VER
        // MSVC has `__FUNCSIG__` for the function signature
        #define assert_always(e) ((void) (_usuallyTrue(!!(e)) ? ((void)0) : fleece::_assert_failed (#e, __FUNCSIG__, __FILE__, __LINE__)))
        #define precondition(e) ((void)  (_usuallyTrue(!!(e)) ? ((void)0) : fleece::_precondition_failed (#e, __FUNCSIG__, __FILE__, __LINE__)))
        #define postcondition(e) ((void) (_usuallyTrue(!!(e)) ? ((void)0) : fleece::_postcondition_failed (#e, __FUNCSIG__, __FILE__, __LINE__)))
    #elif defined(__FILE_NAME__)
        // Clang has `__FILE_NAME__` for the filename w/o path
        #define assert_always(e) ((void) (_usuallyTrue(!!(e)) ? ((void)0) : fleece::_assert_failed (#e, __PRETTY_FUNCTION__, __FILE_NAME__, __LINE__)))
        #define precondition(e) ((void)  (_usuallyTrue(!!(e)) ? ((void)0) : fleece::_precondition_failed (#e, __PRETTY_FUNCTION__, __FILE_NAME__, __LINE__)))
        #define postcondition(e) ((void) (_usuallyTrue(!!(e)) ? ((void)0) : fleece::_postcondition_failed (#e, __PRETTY_FUNCTION__, __FILE_NAME__, __LINE__)))
    #else
        // GCC/default: use `__PRETTY_FUNCTION__` and `__FILE__`
        #define assert_always(e) ((void) (_usuallyTrue(!!(e)) ? ((void)0) : fleece::_assert_failed (#e, __PRETTY_FUNCTION__, __FILE__, __LINE__)))
        #define precondition(e) ((void)  (_usuallyTrue(!!(e)) ? ((void)0) : fleece::_precondition_failed (#e, __PRETTY_FUNCTION__, __FILE__, __LINE__)))
        #define postcondition(e) ((void) (_usuallyTrue(!!(e)) ? ((void)0) : fleece::_postcondition_failed (#e, __PRETTY_FUNCTION__, __FILE__, __LINE__)))
    #endif

    namespace fleece {
        [[noreturn]] NOINLINE void _assert_failed(const char *condition, const char *fn,
                                                  const char *file, int line);
        [[noreturn]] NOINLINE void _precondition_failed(const char *condition, const char *fn,
                                                        const char *file, int line);
        [[noreturn]] NOINLINE void _postcondition_failed(const char *condition, const char *fn,
                                                         const char *file, int line);

        class assertion_failure : public std::logic_error {
        public:
            assertion_failure(const char *what) :logic_error(what) { }
        };
    }
#endif // assert_always

// `assert()`, `assert_precondition()`, and `assert_postcondition()` are just like the macros
// above, except that they are disabled when `NDEBUG` is defined. They should be used when the
// evaluation of the expression would hurt performance in a release build.

#undef assert
#undef assert_precondition
#undef assert_postcondition
#ifdef NDEBUG
    #define assert(e)               (void(0))
    #define assert_precondition(e)  (void(0))
    #define assert_postcondition(e) (void(0))
#else
    #define assert(e)               assert_always(e)
    #define assert_precondition(e)  precondition(e)
    #define assert_postcondition(e) postcondition(e)
#endif //NDEBUG
