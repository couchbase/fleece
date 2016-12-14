//
//  JSON5Tests.cc
//  Fleece
//
//  Created by Jens Alfke on 12/13/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#include "JSON5.hh"
#include "catch.hpp"

using namespace fleece;


TEST_CASE("JSON5 Constants") {
    CHECK(ConvertJSON5("null") == "null");
    CHECK(ConvertJSON5("false") == "false");
    CHECK(ConvertJSON5("true") == "true");

    CHECK(ConvertJSON5("  true") == "true");
    CHECK(ConvertJSON5("true  ") == "true");
}

TEST_CASE("JSON5 Comments") {
    CHECK(ConvertJSON5("/* comment */true") == "true");
    CHECK(ConvertJSON5("true /* comment */") == "true");
    CHECK(ConvertJSON5("// comment\ntrue") == "true");
    CHECK(ConvertJSON5("true // comment") == "true");
}

TEST_CASE("JSON5 Numbers") {
    CHECK(ConvertJSON5("0") == "0");
    CHECK(ConvertJSON5("1") == "1");
    CHECK(ConvertJSON5("12340") == "12340");
    CHECK(ConvertJSON5("-12340") == "-12340");
    CHECK(ConvertJSON5("+12340") == "12340");
    CHECK(ConvertJSON5("92.876") == "92.876");
    CHECK(ConvertJSON5(".7") == "0.7");
    CHECK(ConvertJSON5("6.02e23") == "6.02e23");
    CHECK(ConvertJSON5("6.02E+23") == "6.02E+23");
    CHECK(ConvertJSON5("6.02E-23") == "6.02E-23");
}

TEST_CASE("JSON5 Strings") {
    CHECK(ConvertJSON5("\"hi\"") == "\"hi\"");
    CHECK(ConvertJSON5("'hi \\\nthere'") == "\"hi there\"");
    CHECK(ConvertJSON5("\"hi \\\"there\\\"\"") == "\"hi \\\"there\\\"\"");
    CHECK(ConvertJSON5("'hi'") == "\"hi\"");
    CHECK(ConvertJSON5("'hi \"there\"'") == "\"hi \\\"there\\\"\"");
    CHECK(ConvertJSON5("'can\\'t'") == "\"can't\"");
}

TEST_CASE("JSON5 Arrays") {
    CHECK(ConvertJSON5("[]") == "[]");
    CHECK(ConvertJSON5("[1]") == "[1]");
    CHECK(ConvertJSON5("[1,2, 3]") == "[1,2,3]");
    CHECK(ConvertJSON5("[1,2, 3,]") == "[1,2,3]");
    CHECK(ConvertJSON5("[1,[2,3],'hi',]") == "[1,[2,3],\"hi\"]");
}

TEST_CASE("JSON5 Objects") {
    CHECK(ConvertJSON5("{}") == "{}");
    CHECK(ConvertJSON5("{\"key\":false}") == "{\"key\":false}");
    CHECK(ConvertJSON5("{'key':false}") == "{\"key\":false}");
    CHECK(ConvertJSON5("{'key':false,}") == "{\"key\":false}");
    CHECK(ConvertJSON5("{key:false,$other:'hey',}") == "{\"key\":false,\"$other\":\"hey\"}");
    CHECK(ConvertJSON5("{_key : false, _Oth3r:null,}") == "{\"_key\":false,\"_Oth3r\":null}");
}
