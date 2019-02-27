//
// PerfTests.cc
//
// Copyright (c) 2015 Couchbase, Inc All rights reserved.
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
#include "FleeceImpl.hh"
#include "JSONConverter.hh"
#include "Doc.hh"
#include "varint.hh"
#include <chrono>
#include <stdlib.h>
#include <thread>
#ifndef _MSC_VER
#include <unistd.h>
#endif

// Catch's REQUIRE is too slow for perf testing
#undef REQUIRE
#define REQUIRE(TEST)   while (!(TEST)) {abort();}
#undef CHECK
#define CHECK(TEST)     while (!(TEST)) {abort();}
#ifdef __APPLE__
#define FLEECE_UNUSED __unused
#elif !defined(_MSC_VER)
#define FLEECE_UNUSED __attribute__((unused))
#else
#define FLEECE_UNUSED
#endif


using namespace fleece;
using namespace fleece::impl;


#if !FL_EMBEDDED


TEST_CASE("GetUVarint performance", "[.Perf]") {
    static constexpr int kNRounds = 10000000;
    Benchmark bench;
    uint8_t buf[100];
    fprintf(stderr, "buf = %p\n", &buf);
    double d = 1.0;
    while (d <= UINT64_MAX) {
        auto n = (uint64_t)d;
        size_t nBytes = PutUVarInt(buf, n);
        uint64_t result = 0;
        bench.start();
        for (int round = 0; round < kNRounds; ++round) {
            uint64_t nn;
            CHECK(GetUVarInt(slice(buf, sizeof(buf)), &nn) == nBytes);
            result += nn;
        }
        bench.stop();
        CHECK(result != 1); // bogus
        fprintf(stderr, "n = %16llx; %2zd bytes; time = %.3f ns\n",
                n, nBytes,
                bench.elapsed() / kNRounds * 1.0e9);
         d *= 1.5;
    }
    bench.printReport(1.0/kNRounds);
}

TEST_CASE("Perf Convert1000People", "[.Perf]") {
    static const int kSamples = 500;

    std::vector<double> elapsedTimes;
    auto input = readTestFile(kBigJSONTestFileName);

    Benchmark bench;

    alloc_slice lastResult;
    fprintf(stderr, "Converting JSON to Fleece...\n");
    for (int i = 0; i < kSamples; i++) {
        bench.start();
        {
            Encoder e(input.size);
            e.uniqueStrings(true);
            JSONConverter jr(e);

            jr.encodeJSON(input);
            e.end();
            auto result = e.finish();
            if (i == kSamples-1)
                lastResult = result;
        }
        bench.stop();

        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    bench.printReport();

    fprintf(stderr, "\nJSON size: %zu bytes; Fleece size: %zu bytes (%.2f%%)\n",
            input.size, lastResult.size, (lastResult.size*100.0/input.size));
    writeToFile(lastResult, kTestFilesDir "1000people.fleece");
}

TEST_CASE("Perf LoadFleece", "[.Perf]") {
    static const int kIterations = 1000;
    auto doc = readTestFile("1000people.fleece");

    {
        fprintf(stderr, "Scanning untrusted Fleece... ");
        Benchmark bench;
        for (int i = 0; i < kIterations; i++) {
            bench.start();
            FLEECE_UNUSED auto root = Value::fromData(doc)->asArray();
            REQUIRE(root != nullptr);
            bench.stop();
        }
        bench.printReport();
    }

    {
        fprintf(stderr, "Scanning trusted Fleece... ");
        static const int kIterationsPerSample = 1000000;
        Benchmark bench;
        for (int i = 0; i < kIterations; i++) {
            bench.start();
            for (int j = 0; j < kIterationsPerSample; j++) {
                FLEECE_UNUSED auto root = Value::fromTrustedData(doc)->asArray();
                REQUIRE(root != nullptr);
            }
            bench.stop();
        }
        bench.printReport(1.0/kIterationsPerSample);
    }
}

static void testFindPersonByIndex(int sort) {
    int kSamples = 500;
    int kIterations = 10000;
    Benchmark bench;

    auto doc = readTestFile("1000people.fleece");

    Dict::key nameKey(slice("name"));

    fprintf(stderr, "Looking up one value, sorted=%d...\n", sort);
    for (int i = 0; i < kSamples; i++) {
        bench.start();

        for (int j = 0; j < kIterations; j++) {
            auto root = Value::fromTrustedData(doc)->asArray();
            auto person = root->get(123)->asDict();
            const Value *name;
            switch (sort) {
                case 1: name = person->get(slice("name")); break;
                case 2: name = person->get(nameKey); break;
                default: return;
            }
            std::string nameStr = (std::string)name->asString();
#ifndef NDEBUG
            REQUIRE(nameStr == std::string("Concepcion Burns"));
#endif
        }

        bench.stop();
    }
    bench.printReport(1.0/kIterations);
}

TEST_CASE("Perf FindPersonByIndexSorted", "[.Perf]")      {testFindPersonByIndex(1);}
TEST_CASE("Perf FindPersonByIndexKeyed", "[.Perf]")       {testFindPersonByIndex(2);}

TEST_CASE("Perf LoadPeople", "[.Perf]") {
    for (int shareKeys = false; shareKeys <= true; ++shareKeys) {
        int kSamples = 50;
        int kIterations = 1000;
        Benchmark bench;

        auto data = readTestFile("1000people.fleece");
        auto sk = retained(new SharedKeys);

        if (shareKeys) {
            Encoder enc;
            enc.setSharedKeys(sk);
            enc.writeValue(Value::fromTrustedData(data));
            data = enc.finish();
        }

        Dict::key keys[10] = {
            {"about"_sl},
            {"age"_sl},
            {"balance"_sl},
            {"guid"_sl},
            {"isActive"_sl},
            {"latitude"_sl},
            {"longitude"_sl},
            {"name"_sl},
            {"registered"_sl},
            {"tags"_sl},
        };


        fprintf(stderr, "Looking up 1000 people (with%s shared keys)...\n", (shareKeys ? "" : "out"));
        for (int i = 0; i < kSamples; i++) {
            bench.start();

            for (int j = 0; j < kIterations; j++) {
                auto doc = retained(new Doc(data, Doc::kTrusted, sk));
                auto root = doc->root()->asArray();
                for (Array::iterator iter(root); iter; ++iter) {
                    const Dict *person = iter->asDict();
                    size_t n = 0;
                    for (int k = 0; k < 10; k++)
                        if (person->get(keys[k]) != nullptr)
                            n++;
                    REQUIRE(n == 10);
                }
            }

            bench.stop();
        }
        bench.printReport(1.0/kIterations, "person");
    }
}


TEST_CASE("Perf DictSearch", "[.Perf]") {
    static const int kSamples = 500000;

    // Convert JSON array into a dictionary keyed by _id:
    alloc_slice input = readTestFile("1000people.fleece");
    if (!input)
        abort();
    std::vector<alloc_slice> names;
    unsigned nPeople = 0;
    Encoder enc;
    enc.beginDictionary();
    for (Array::iterator i(Value::fromTrustedData(input)->asArray()); i; ++i) {
        auto person = i.value()->asDict();
        auto key = person->get("guid"_sl)->asString();
        enc.writeKey(key);
        enc.writeValue(person);
        names.emplace_back(key);
        if (++nPeople >= 1000)
            break;
    }
    enc.endDictionary();
    alloc_slice dictData = enc.finish();
    auto people = Value::fromTrustedData(dictData)->asDict();

    Benchmark bench;

    for (int i = 0; i < kSamples; i++) {
        slice keys[100];
        for (int k = 0; k < 100; k++)
            keys[k] = names[ random() % names.size() ];
        bench.start();
        {
            for (int k = 0; k < 100; k++) {
                const Value *person = people->get(keys[k]);
                if (!person)
                    abort();
            }
        }
        bench.stop();

        //std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    bench.printReport();
}

#endif // !FL_EMBEDDED
