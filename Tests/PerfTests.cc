//
//  PerfTests.cpp
//  Fleece
//
//  Created by Jens Alfke on 11/21/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//

#include "FleeceTests.hh"
#include "JSONConverter.hh"
#include <assert.h>
#include <unistd.h>

using namespace fleece;

class PerfTests : public CppUnit::TestFixture {
    public:

    static const bool kSortKeys = true;

    void testConvert1000People() {
        static const int kSamples = 500;

        std::vector<double> elapsedTimes;
        alloc_slice input = readFile(kTestFilesDir "1000people.json");

        Benchmark bench;

        alloc_slice lastResult;
        fprintf(stderr, "Converting JSON to Fleece...\n");
        for (int i = 0; i < kSamples; i++) {
            bench.start();
            {
                Writer writer(input.size);
                Encoder e(writer);
                e.uniqueStrings(true);
                e.sortKeys(kSortKeys);
                JSONConverter jr(e);

                jr.convertJSON(input);
                e.end();
                auto result = writer.extractOutput();
                if (i == kSamples-1)
                    lastResult = result;
            }
            bench.stop();

            usleep(100);
        }
        bench.printReport(1000, "ms");

        fprintf(stderr, "\nJSON size: %zu bytes; Fleece size: %zu bytes (%.2f%%)\n",
                input.size, lastResult.size, (lastResult.size*100.0/input.size));
        writeToFile(lastResult, kTestFilesDir "1000people.fleece");
    }

    void testLoadFleece() {
        static const int kIterations = 1000;
        alloc_slice doc = readFile(kTestFilesDir "1000people.fleece");

        {
            fprintf(stderr, "Scanning untrusted Fleece... ");
            Benchmark bench;
            for (int i = 0; i < kIterations; i++) {
                bench.start();
                __unused auto root = value::fromData(doc)->asArray();
                Assert(root != NULL);
                bench.stop();
            }
            bench.printReport(1e3, "ms");
        }

        {
            fprintf(stderr, "Scanning trusted Fleece... ");
            static const int kIterationsPerSample = 1000;
            Benchmark bench;
            for (int i = 0; i < kIterations; i++) {
                bench.start();
                for (int j = 0; j < kIterationsPerSample; j++) {
                    __unused auto root = value::fromTrustedData(doc)->asArray();
                    Assert(root != NULL);
                }
                bench.stop();
            }
            bench.printReport(1e6 / kIterationsPerSample, "µs");
        }
}

    void testFindPersonByIndex(int sort) {
        int kSamples = 500;
        int kIterations = 10000;
        Benchmark bench;

        mmap_slice doc(kTestFilesDir "1000people.fleece");

        dict::key nameKey(slice("name"));

        fprintf(stderr, "Looking up one value, sorted=%d...\n", sort);
        for (int i = 0; i < kSamples; i++) {
            bench.start();

            for (int j = 0; j < kIterations; j++) {
                auto root = value::fromTrustedData(doc)->asArray();
                auto person = root->get(123)->asDict();
                const value *name;
                switch (sort) {
                    case 0: name = person->get_unsorted(slice("name")); break;
                    case 1: name = person->get(slice("name")); break;
                    case 2: name = person->get(nameKey); break;
                    default: return;
                }
                std::string nameStr = (std::string)name->asString();
#ifndef NDEBUG
                AssertEqual(nameStr, std::string("Concepcion Burns"));
#endif
            }

            bench.stop();
        }
        bench.printReport(1e9 / kIterations, "ns");
    }

    void testFindPersonByIndexUnsorted()    {testFindPersonByIndex(0);}
    void testFindPersonByIndexSorted()      {if (kSortKeys) testFindPersonByIndex(1);}
    void testFindPersonByIndexKeyed()       {testFindPersonByIndex(2);}

    CPPUNIT_TEST_SUITE( PerfTests );
    CPPUNIT_TEST( testConvert1000People );
    CPPUNIT_TEST( testLoadFleece );
    CPPUNIT_TEST( testFindPersonByIndexUnsorted );
    CPPUNIT_TEST( testFindPersonByIndexSorted );
    CPPUNIT_TEST( testFindPersonByIndexKeyed );
    CPPUNIT_TEST_SUITE_END();
};

#ifdef NDEBUG
CPPUNIT_TEST_SUITE_REGISTRATION(PerfTests);
#endif
