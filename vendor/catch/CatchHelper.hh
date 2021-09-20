//
//  CatchHelper.hh
//  LiteCore
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


// N_WAY_TEST_CASE_METHOD is like TEST_CASE_METHOD, except the constructor of the test
// class must take a boolean parameter, and the test case will be run twice, once on an object
// that's been constructed with a 'false' parameter, and once with 'true'.



namespace Catch {
    template<typename C>
    class NWayMethodTestInvoker : public ITestInvoker {

    public:
        NWayMethodTestInvoker( void (C::*method)() ) : m_method( method ) {}

        virtual void invoke() const {
            {
                for (int i = 0; i < C::numberOfOptions; i++) {
                    C obj(i);
                    (obj.*m_method)();
                }
            }
        }

    private:
        void (C::*m_method)();
    };
}

#define N_WAY_TEST_CASE_METHOD2( TestName, ClassName, ... )\
    namespace{ \
        struct TestName : INTERNAL_CATCH_REMOVE_PARENS(ClassName) { \
            TestName(int opt) :ClassName(opt) { } \
            void test(); \
        }; \
        Catch::AutoReg INTERNAL_CATCH_UNIQUE_NAME( autoRegistrar ) ( new Catch::NWayMethodTestInvoker( &TestName::test ), CATCH_INTERNAL_LINEINFO, #ClassName, Catch::NameAndTags{ __VA_ARGS__ } ); \
    } \
    void TestName::test()

#define N_WAY_TEST_CASE_METHOD( ClassName, ... ) \
    N_WAY_TEST_CASE_METHOD2( INTERNAL_CATCH_UNIQUE_NAME( ____C_A_T_C_H____T_E_S_T____ ), ClassName, __VA_ARGS__ )
