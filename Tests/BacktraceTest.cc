//
// BacktraceTest.cc
//
// Copyright © 2021 Couchbase. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "FleeceTests.hh"
#include "Backtrace.hh"

using namespace fleece;

TEST_CASE("Backtrace print", "[Backtrace]") {
    auto bt = Backtrace::capture();
    auto trace = bt->toString();
    CHECK(trace.find("more suppressed") != std::string::npos);
}

#ifdef DEBUG

namespace test::backtrace {
    static int* createInvalidRef() {
        return nullptr;
    }

    static void doBadThings() {
        int* landline = createInvalidRef();
        // ReSharper disable once CppDFANullDereference
        *landline = 0xdead;
    }
    static void crashOnPurpose() {
        doBadThings();
    }
}

TEST_CASE("Backtrace crash", "[BacktraceManual]") {
    // Since this test crashes the process intentionally,
    // It will fail and require manual inspection of stderr
    test::backtrace::crashOnPurpose();
}
#endif