//
//  CatchHelper.hh
//  LiteCore
//
//  Created by Jens Alfke on 8/31/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once

#include "catch.hpp"


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
