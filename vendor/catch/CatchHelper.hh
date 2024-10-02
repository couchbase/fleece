//
//  CatchHelper.hh
//
//  Created by Jens Alfke on 8/31/16.
//  Copyright 2016-Present Couchbase, Inc.
//
//  Use of this software is governed by the Business Source License included
//  in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
//  in that file, in accordance with the Business Source License, use of this
//  software will be governed by the Apache License, Version 2.0, included in
//  the file licenses/APL2.txt.
//

#pragma once

#include "catch.hpp"


#pragma mark - EXTRA ASSERTION MACROS


// `CHECKED` evaluates its parameter, asserts that it's not false/0/null, and returns it.
// Like `CHECK`, failure does _not_ abort the current test.
#define CHECKED(EXPR) _CHECKED(EXPR, "Failed " #EXPR)

// `REQUIRED` evaluates its parameter, asserts that it's not false/0/null, and returns it.
// Like `REQUIRE`, failure aborts the current test.
#define REQUIRED(EXPR) _REQUIRED(EXPR, "Failed " #EXPR)

template <class T>
inline T _CHECKED(T &&value, const char *expr) {if (!value) FAIL_CHECK(expr); return value; }
template <class T>
inline T _REQUIRED(T &&value, const char *expr) {if (!value) FAIL(expr); return value; }


#pragma mark - N-WAY TEST METHODS


namespace Catch {
    template<typename C>
    class NWayMethodTestInvoker : public ITestInvoker {
    public:
        NWayMethodTestInvoker( void (C::*method)() ) : m_method( method ) {}

        virtual void invoke() const {
            int option = GENERATE(range(0, C::numberOfOptions));
            DYNAMIC_SECTION("Option " << option << ": " << C::nameOfOption[option]) {
                auto obj = C(option);
                (obj.*m_method)();
            }
        }

    private:
        void (C::*m_method)();
    };
}


/// N_WAY_TEST_CASE_METHOD is like TEST_CASE_METHOD, except
/// - the class must have these two members:
///   - `static constexpr int numberOfOptions`
///   - `static constexpr const char* nameOfOption[numberOfOptions]`
/// - the constructor of the test class must take an int parameter
/// - the test case will be run `numberOfOptions` times
/// - each test instance will be constructed with an integer ranging from 0 to `numberOfOptions - 1`
#define N_WAY_TEST_CASE_METHOD2( TestName, ClassName, ... )\
    CATCH_INTERNAL_START_WARNINGS_SUPPRESSION \
    CATCH_INTERNAL_SUPPRESS_GLOBALS_WARNINGS \
    CATCH_INTERNAL_SUPPRESS_UNUSED_VARIABLE_WARNINGS \
    namespace{ \
        struct TestName : INTERNAL_CATCH_REMOVE_PARENS(ClassName) { \
            explicit TestName(int opt) :ClassName(opt) { } \
            void test(); \
        }; \
        const Catch::AutoReg INTERNAL_CATCH_UNIQUE_NAME( autoRegistrar )( \
        Catch::Detail::make_unique<Catch::NWayMethodTestInvoker<TestName>>( &TestName::test ), \
        CATCH_INTERNAL_LINEINFO,                                      \
        #ClassName##_catch_sr,                                        \
        Catch::NameAndTags{ __VA_ARGS__ } ); /* NOLINT */ \
    } \
    CATCH_INTERNAL_STOP_WARNINGS_SUPPRESSION \
    void TestName::test()


#define N_WAY_TEST_CASE_METHOD( ClassName, ... ) \
    N_WAY_TEST_CASE_METHOD2( INTERNAL_CATCH_UNIQUE_NAME( CATCH2_INTERNAL_TEST_ ), ClassName, __VA_ARGS__ )
