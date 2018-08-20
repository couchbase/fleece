//
// betterassert.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

// This is an alternate implementation of assert() that produces a nicer message that includes
// the function name, and throws a C++ exception instead of calling abort().
// NOTE: If <assert.h> is included later than this header, it will overwrite this definition of
// assert() with the usual one. (And vice versa.)

#include <stdexcept>

#ifndef NDEBUG

    #ifndef betterassert
        #ifdef _MSC_VER
            #define betterassert(e) ((void) ((e) ? ((void)0) : fleece::_assert_failed (#e, __FUNCSIG__, __FILE__, __LINE__)))
        #else
            #define betterassert(e) ((void) ((e) ? ((void)0) : fleece::_assert_failed (#e, __PRETTY_FUNCTION__, __FILE__, __LINE__)))
        #endif

        namespace fleece {
            [[noreturn]] void _assert_failed(const char *condition, const char *fn,
                                             const char *file, int line);
            
            class assertion_failure : public std::logic_error {
            public:
                assertion_failure(const char *what) :logic_error(what) { }
            };
        }
    #endif // betterassert

    #undef assert
    #define assert(e) betterassert(e)

#endif //NDEBUG
