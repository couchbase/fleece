//
// NumericTests.cc
//
// Copyright 2019-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

// Adapted from Swift tests at
// https://github.com/apple/swift/blob/master/test/stdlib/PrintFloat.swift.gyb
// (There are many more that haven't been ported yet)

#include "FleeceTests.hh"
#include "NumConversion.hh"
#include <cmath>
#include <limits>
#include <string>

using namespace std;


template <class FLOAT>
static string floatStr(FLOAT n) {
    char str[32];
    auto length = WriteFloat(n, str, sizeof(str));
    CHECK(length == strlen(str));
    if (sizeof(FLOAT) >= sizeof(double)) {
        // Test for 100% accurate double->string->double round-trip:
        CHECK(ParseDouble(str) == n);
    }
    return string(str);
}


#define expectDescription(STR, N)   CHECK(floatStr(N) == STR)


TEST_CASE("WriteFloat","[Numeric]") {
    static constexpr float maxDecimalForm = float(1 << 24);

    expectDescription("0.0", 0.0F);
    expectDescription("-0.0", -0.0F);
    expectDescription("0.1", 0.1F);
    expectDescription("-0.1", -0.1F);
    expectDescription("1.0", 1.0F);
    expectDescription("-1.0", -1.0F);
    expectDescription("1.1", 1.1F);
    expectDescription("100.125", 100.125F);
    expectDescription("-100.125", -100.125F);
    expectDescription("inf", numeric_limits<float>::infinity());
    expectDescription("-inf", -numeric_limits<float>::infinity());
    expectDescription("3.1415925", 3.1415926f);
    expectDescription("3.4028235e+38", numeric_limits<float>::max());
    expectDescription("1e-45", numeric_limits<float>::denorm_min());
    expectDescription("1.1754944e-38", numeric_limits<float>::min());
    expectDescription("1.00000075e-36", 1.00000075e-36F);
    expectDescription("7.0385313e-26", 7.0385313e-26F);
    expectDescription("16777216.0", maxDecimalForm);
    expectDescription("-16777216.0", -maxDecimalForm);
    expectDescription("1.6777218e+07", nextafter(maxDecimalForm, HUGE_VALF));
    expectDescription("-1.6777218e+07", nextafter(-maxDecimalForm, -HUGE_VALF));
    expectDescription("1.00001", 1.00001F);
    expectDescription("1.25e+17", 125000000000000000.0F);
    expectDescription("1.25e+16", 12500000000000000.0F);
    expectDescription("1.25e+15", 1250000000000000.0F);
    expectDescription("1.25e+14", 125000000000000.0F);
    expectDescription("1.25e+13", 12500000000000.0F);
    expectDescription("1.25e+12", 1250000000000.0F);
    expectDescription("1.25e+11", 125000000000.0F);
    expectDescription("1.25e+10", 12500000000.0F);
    expectDescription("1.25e+09", 1250000000.0F);
    expectDescription("1.25e+08", 125000000.0F);
    expectDescription("12500000.0", 12500000.0F);
    expectDescription("1250000.0", 1250000.0F);
    expectDescription("125000.0", 125000.0F);
    expectDescription("12500.0",  12500.0F);
    expectDescription("1250.0",   1250.0F);
    expectDescription("125.0",    125.0F);
    expectDescription("12.5",     12.5F);
    expectDescription("1.25",     1.25F);
    expectDescription("0.125",    0.125F);
    expectDescription("0.0125",   0.0125F);
    expectDescription("0.00125",  0.00125F);
    expectDescription("0.000125", 0.000125F);
    expectDescription("1.25e-05", 0.0000125F);
    expectDescription("1.25e-06", 0.00000125F);
    expectDescription("1.25e-07", 0.000000125F);
    expectDescription("1.25e-08", 0.0000000125F);
    expectDescription("1.25e-09", 0.00000000125F);
    expectDescription("1.25e-10", 0.000000000125F);
    expectDescription("1.25e-11", 0.0000000000125F);
    expectDescription("1.25e-12", 0.00000000000125F);
    expectDescription("1.25e-13", 0.000000000000125F);
    expectDescription("1.25e-14", 0.0000000000000125F);
    expectDescription("1.25e-15", 0.00000000000000125F);
    expectDescription("1.25e-16", 0.000000000000000125F);
    expectDescription("1.25e-17", 0.0000000000000000125F);
}


TEST_CASE("WriteDouble","[Numeric]") {
    static constexpr double maxDecimalForm = double(1LL << 53);

    expectDescription("0.0", 0.0);
    expectDescription("-0.0", -0.0);
    expectDescription("0.1", 0.1);
    expectDescription("-0.1", -0.1);
    expectDescription("1.0", 1.0);
    expectDescription("-1.0", -1.0);
    expectDescription("1.1", 1.1);
    expectDescription("100.125", 100.125);
    expectDescription("-100.125", -100.125);
    expectDescription("3.141592653589793", M_PI);
    expectDescription("1.7976931348623157e+308", numeric_limits<double>::max());
    expectDescription("5e-324", numeric_limits<double>::denorm_min());
    expectDescription("2.2250738585072014e-308", numeric_limits<double>::min());
    expectDescription("inf", numeric_limits<double>::infinity());
    expectDescription("-inf", -numeric_limits<double>::infinity());
    expectDescription("2.311989689387339e-82", 2.311989689387339e-82);
    expectDescription("9007199254740992.0", maxDecimalForm);
    expectDescription("-9007199254740992.0", -maxDecimalForm);
    expectDescription("9.007199254740994e+15", nextafter(maxDecimalForm, HUGE_VAL));
    expectDescription("-9.007199254740994e+15", nextafter(-maxDecimalForm, -HUGE_VAL));
    expectDescription("1.00000000000001", 1.00000000000001);
    expectDescription("1.25e+17", 125000000000000000.0);
    expectDescription("1.25e+16", 12500000000000000.0);
    expectDescription("1250000000000000.0", 1250000000000000.0);
    expectDescription("125000000000000.0", 125000000000000.0);
    expectDescription("12500000000000.0", 12500000000000.0);
    expectDescription("1250000000000.0", 1250000000000.0);
    expectDescription("125000000000.0", 125000000000.0);
    expectDescription("12500000000.0", 12500000000.0);
    expectDescription("1250000000.0", 1250000000.0);
    expectDescription("125000000.0", 125000000.0);
    expectDescription("12500000.0", 12500000.0);
    expectDescription("1250000.0", 1250000.0);
    expectDescription("125000.0", 125000.0);
    expectDescription("12500.0", 12500.0);
    expectDescription("1250.0", 1250.0);
    expectDescription("125.0", 125.0);
    expectDescription("12.5", 12.5);
    expectDescription("1.25", 1.25);
    expectDescription("0.125", 0.125);
    expectDescription("0.0125", 0.0125);
    expectDescription("0.00125", 0.00125);
    expectDescription("0.000125", 0.000125);
    expectDescription("1.25e-05", 0.0000125);
    expectDescription("1.25e-06", 0.00000125);
    expectDescription("1.25e-07", 0.000000125);
    expectDescription("1.25e-08", 0.0000000125);
    expectDescription("1.25e-09", 0.00000000125);
    expectDescription("1.25e-10", 0.000000000125);
    expectDescription("1.25e-11", 0.0000000000125);
    expectDescription("1.25e-12", 0.00000000000125);
    expectDescription("1.25e-13", 0.000000000000125);
    expectDescription("1.25e-14", 0.0000000000000125);
    expectDescription("1.25e-15", 0.00000000000000125);
    expectDescription("1.25e-16", 0.000000000000000125);
    expectDescription("1.25e-17", 0.0000000000000000125);
}
