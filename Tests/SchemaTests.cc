//
// SchemaTests.cc
//
// Copyright 2025-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "fleece/JSONSchema.hh"
#include "JSON5.hh"
#include "FleeceTests.hh"
#include <fstream>
#include <future>
#include <iostream>
#include <optional>

using namespace std;
using namespace fleece;


struct SchemaTest {
    void setSchema(string const& json5) {
        schema.emplace(ConvertJSON5(json5));
    }

    void checkValid(string const& json5) {
        Doc test = Doc::fromJSON(ConvertJSON5(json5));
        if (auto val = schema.value().validate(test.root()); !val) {
            INFO("error = " << val.errorString() << ", path = " << val.errorPath());
            FAIL_CHECK("Failed to validate: " << json5);
            CHECK(val.error() == JSONSchema::Error::ok);
            CHECK(val.errorString() == "ok");
            CHECK(val.errorPath() == "");
            CHECK(val.errorValue() == nullptr);
        }
    }

    void checkInvalid(string const& json5, JSONSchema::Error expectedErr,
                    string_view path, string_view badJSON, string_view schemaJSON, string_view schemaURI) {
        Doc test = Doc::fromJSON(ConvertJSON5(json5));
        if (auto val = schema.value().validate(test.root()); val) {
            FAIL_CHECK("Failed to detect invalid: " << json5);
        } else {
            INFO("doc = " << json5 << ", path = " << val.errorPath() << ", val = " << val.errorValue().toJSONString());
            CHECK(val.error() == expectedErr);
            CHECK(val.errorString() == JSONSchema::errorString(expectedErr));
            CHECK(val.errorPath() == path);
            CHECK(val.errorValue().toJSONString() == badJSON);
            CHECK(val.errorSchema().second.toJSONString() == schemaJSON);
            CHECK(val.errorSchemaURI() == schemaURI);
        }
    }

    optional<JSONSchema> schema;
};

TEST_CASE_METHOD(SchemaTest, "JSON Schema", "[Schema]") {
    using enum JSONSchema::Error;
    setSchema("{type: 'object', properties: {'str': {type: 'string'}, 'arr': {items: {enum: [1,2]}} }}");

    checkValid("{}");
    checkValid("{str: 'foo'}");
    checkValid("{xxx: false, yyy: true}");
    checkInvalid("[]", typeMismatch,
        "$",     "[]", "\"object\"", "#/type");
    checkInvalid("{str: 17}", typeMismatch,
        "$.str", "17", "\"string\"", "#/properties/str/type");
    checkInvalid("{str: 'bar', arr: [1, 2, 3.5]}", notEnum,
        "$.arr[2]", "3.5", "[1,2]", "#/properties/arr/items/enum");
}


TEST_CASE_METHOD(SchemaTest,"JSON Schema Test Suite", "[Schema]") {
    // https://github.com/json-schema-org/JSON-Schema-Test-Suite
    // NOTE: Test files that exclusively test features we don't support are commented out below.
#ifdef _WIN32
    static constexpr const char* kTestSuitePath = "../vendor\\JSON-Schema-Test-Suite\\tests\\draft2020-12\\";
#else
    static constexpr const char* kTestSuitePath = "../vendor/JSON-Schema-Test-Suite/tests/draft2020-12/";
#endif
    string testsDir = string(kTestFilesDir) + kTestSuitePath;

    static constexpr const char* kTestFiles[] = {
        "additionalProperties",
        "allOf",
        "anchor",
        "anyOf",
        "boolean_schema",
        "const",
        "contains",
        "content",
        "default",
        // "defs",
        "dependentRequired",
        "dependentSchemas",
        // "dynamicRef",
        "enum",
        "exclusiveMaximum",
        "exclusiveMinimum",
        "format",
        "if-then-else",
        "infinite-loop-detection",
        "items",
        "maxContains",
        "maxItems",
        "maxLength",
        "maxProperties",
        "maximum",
        "minContains",
        "minItems",
        "minLength",
        "minProperties",
        "minimum",
        "multipleOf",
        "not",
        "oneOf",
        "pattern",
        "patternProperties",
        "prefixItems",
        "properties",
        "propertyNames",
        "ref",
        // "refRemote",
        "required",
        "type",
        "unevaluatedItems",
        "unevaluatedProperties",
        "uniqueItems",
        // "vocabulary",
    };

    // Some individual tests that are known to fail, so we skip them:
    static constexpr string_view kSkipTests[] = {
        "enum/enum with [0] does not match [false]/[0.0] is valid",
        "enum/enum with [1] does not match [true]/[1.0] is valid",
        "ref/remote ref, containing refs itself/remote ref valid", //TODO: Unsure why this fails
    };

    // In addition, any test that throws `unsupported_schema` is skipped.

    for (auto filename : kTestFiles) {
        DYNAMIC_SECTION(filename) {
            Doc tests = Doc::fromJSON(readFile((testsDir + filename + ".json").c_str()));
            for (Array::iterator i(tests.asArray()); i; ++i) {
                Dict group = i.value().asDict();
                string_view groupName(group["description"].asString());
                DYNAMIC_SECTION(groupName) {
                    INFO("Schema: " << group["schema"].toJSONString());
                    try {
                        schema.emplace(group["schema"]);
                        for (Array::iterator j(group["tests"].asArray()); j; ++j) {
                            Dict test = j.value().asDict();
                            string_view testName(test["description"].asString());
                            DYNAMIC_SECTION(testName) {
                                string fullTestName = string(filename) + '/' + string(groupName) + '/' + string(testName);
                                if (ranges::find(kSkipTests, fullTestName) != ranges::end(kSkipTests)) {
                                    SKIP("Skipping known-bad test " << fullTestName);
                                } else {
                                    Value data = test["data"];

                                    auto val = schema->validate(data);    // SHAZAM!

                                    if (val.ok() != test["valid"].asBool()) {
                                        INFO("Test name: " << fullTestName);
                                        INFO("Error: " << val.errorString() << " at " << val.errorPath());
                                        FAIL_CHECK(
                                            (val ? "Should have rejected " : "Should have accepted ")
                                            << data.toJSONString() );
                                    }
                                }
                            }
                        }
                    } catch (JSONSchema::unsupported_schema const& x) {
                        // Skip tests that use unsupported JSON Schema features
                        SKIP("Skipping schema '" << filename << "/" << groupName << "': " << x.what());
                    }
                }
            }
        }
    }
}


TEST_CASE("JSON Schema benchmark", "[.Perf]") {
    static constexpr const char* kDataFile = "/Users/snej/Couchbase/DataSets/travel-sample/travel.json";
    vector<Doc> database;
    {
        Benchmark bench;
        ifstream in(kDataFile);
        REQUIRE(in.good());
        string line;
        while (!in.eof()) {
            getline(in, line);
            if (line.empty())
                continue;
            INFO("JSON = " << line);
            bench.start();
            auto doc = Doc::fromJSON(line);
            bench.stop();
            REQUIRE(doc);
            database.push_back(std::move(doc));
            #ifndef NDEBUG
            //if (database.size() > 2000) {break;} // speeds up debugging
            #endif
        }
        fprintf(stderr, "Read %zu documents:    ", database.size());
        bench.printReport(1.0, "document");
    }

    JSONSchema schema(readFile((string(kTestFilesDir) + "travel-schema.json").c_str()));

    SECTION("Single-threaded") {
        Benchmark bench;
        for (auto& doc : database) {
            bench.start();
            auto result = schema.validate(doc.root());
            bench.stop();
            if (!result) {
                slice id = doc.asDict()["_id"].asString();
                FAIL("Doc " << id << " failed: " << result.errorString() << " at " << result.errorPath()
                     << " (" << result.errorValue().toJSONString() << "), schema at " << result.errorSchemaURI());
            }
        }
        fprintf(stderr, "Checked %zu documents: ", database.size());
        bench.printReport(1.0, "document");
    }

    SECTION("Parallel") {
        static const size_t kBatchSize = (database.size() + 15) / 16;
        size_t const n = database.size();
        vector<future<void>> futures;
        Benchmark bench;
        bench.start();
        for (size_t taskFirst = 0; taskFirst < n; taskFirst += kBatchSize) {
            futures.emplace_back( async(function([&](size_t first) {
                size_t last = std::min(first + kBatchSize, n);
                for (size_t i = first; i < last; ++i) {
                    auto result = schema.validate(database[i].root());
                    if (!result)
                        throw runtime_error("Validation failed!");
                }
            }), taskFirst));
        }
        for (auto& f : futures) f.wait();
        bench.stop();
        fprintf(stderr, "Checked %zu documents: ", database.size());
        bench.printReport(1.0 / n, "document");
    }
}
