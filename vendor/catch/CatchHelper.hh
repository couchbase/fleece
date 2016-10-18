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
    class NWayMethodTestCase : public SharedImpl<ITestCase> {

    public:
        NWayMethodTestCase( void (C::*method)() ) : m_method( method ) {}

        virtual void invoke() const {
            {
                for (int i = 0; i < C::numberOfOptions; i++) {
                    C obj(i);
                    (obj.*m_method)();
                }
            }
        }

    private:
        virtual ~NWayMethodTestCase() {}

        void (C::*m_method)();
    };


    struct NWayAutoReg {

        NWayAutoReg
        (   TestFunction function,
         SourceLineInfo const& lineInfo,
         NameAndDesc const& nameAndDesc );

        template<typename C>
        NWayAutoReg
        (   void (C::*method)(),
         char const* className,
         NameAndDesc const& nameAndDesc,
         SourceLineInfo const& lineInfo ) {

            registerTestCase
                (   new NWayMethodTestCase<C>( method ),
                     className,
                     nameAndDesc,
                     lineInfo );
        }

        ~NWayAutoReg() { }

    private:
        NWayAutoReg( NWayAutoReg const& );
        void operator= ( NWayAutoReg const& );
    };
}

#define N_WAY_TEST_CASE_METHOD2( TestName, ClassName, ... )\
    namespace{ \
        struct TestName : ClassName{ \
            TestName(int opt) :ClassName(opt) { } \
            void test(); \
        }; \
        Catch::NWayAutoReg INTERNAL_CATCH_UNIQUE_NAME( autoRegistrar ) ( &TestName::test, #ClassName, Catch::NameAndDesc( __VA_ARGS__ ), CATCH_INTERNAL_LINEINFO ); \
    } \
    void TestName::test()
#define N_WAY_TEST_CASE_METHOD( ClassName, ... ) \
    N_WAY_TEST_CASE_METHOD2( INTERNAL_CATCH_UNIQUE_NAME( ____C_A_T_C_H____T_E_S_T____ ), ClassName, __VA_ARGS__ )



