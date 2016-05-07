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

        fprintf(stderr, "Converting JSON to Fleece...\n");
        for (int i = 0; i < kSamples; i++) {
            Writer writer(input.size);
            Encoder e(writer);
            e.uniqueStrings(true);
            e.sortKeys(kSortKeys);
            JSONConverter jr(e);
            
            bench.start();

            jr.convertJSON(input);
            e.end();
            auto result = writer.extractOutput();

            bench.stop();

            usleep(100);

            if (i == kSamples-1) {
                fprintf(stderr, "\nJSON size: %zu bytes; Fleece size: %zu bytes (%.2f%%)\n",
                        input.size, result.size, (result.size*100.0/input.size));
                writeToFile(result, kTestFilesDir "1000people.fleece");
            }
        }

        bench.printReport(1000, "ms");
    }

    void testFindPersonByIndex(int sort) {
        int kSamples = 500;
        int kIterations = 10000;
        Benchmark bench;

        mmap_slice doc(kTestFilesDir "1000people.fleece");

        const value *nameKey = NULL;
        if (sort == 2) {
            auto root = value::fromTrustedData(doc)->asArray();
            auto person = root->get(123)->asDict();
            for (auto i=person->begin(); i; ++i) {
                if (i.key()->asString() == slice("name")) {
                    nameKey = i.key();
                    break;
                }
            }
            Assert(nameKey);
        }

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
    CPPUNIT_TEST( testFindPersonByIndexUnsorted );
    CPPUNIT_TEST( testFindPersonByIndexSorted );
    CPPUNIT_TEST( testFindPersonByIndexKeyed );
    CPPUNIT_TEST_SUITE_END();
};

#ifdef NDEBUG
CPPUNIT_TEST_SUITE_REGISTRATION(PerfTests);
#endif
