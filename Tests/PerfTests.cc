//
//  PerfTests.cpp
//  Fleece
//
//  Created by Jens Alfke on 11/21/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//

#include "FleeceTests.hh"
#include "JSONReader.hh"
#include <assert.h>

#define kDir "/Couchbase/Fleece/Tests/"

using namespace fleece;

class PerfTests : public CppUnit::TestFixture {
    public:

    void testConvert1000People() {
        int kSamples = 50;
#ifndef NDEBUG
        kSamples = 1;
#endif

        double total = 0, minTime = 1e99, maxTime = -1;
        alloc_slice input = readFile(kDir "1000people.json");

#ifdef NDEBUG
        fprintf(stderr, "Converting JSON to Fleece (ms):");
#endif
        for (int i = 0; i < kSamples; i++) {
            Writer writer(input.size);
            encoder e(writer);
            e.uniqueStrings(true);
            JSONReader jr(e);
            Stopwatch st;

            jr.writeJSON(input);
            e.end();
            auto result = writer.extractOutput();

            double elapsed = st.elapsedMS();
#ifdef NDEBUG
            fprintf(stderr, " %g", elapsed);
#endif
            total += elapsed;
            minTime = std::min(minTime, elapsed);
            maxTime = std::max(maxTime, elapsed);

            if (i == kSamples-1) {
                fprintf(stderr, "\nJSON size: %zu bytes; Fleece size: %zu bytes (%.2f%%)\n",
                        input.size, result.size, (result.size*100.0/input.size));
                writeToFile(result, "1000people.fleece");
#ifndef NDEBUG
                fprintf(stderr, "Narrow: %u, Wide: %u (total %u)\n", e._numNarrow, e._numWide, e._numNarrow+e._numWide);
                fprintf(stderr, "Narrow count: %u, Wide count: %u (total %u)\n", e._narrowCount, e._wideCount, e._narrowCount+e._wideCount);
                fprintf(stderr, "Used %u pointers to shared strings\n", e._numSavedStrings);
#endif
            }
        }
#ifdef NDEBUG
        fprintf(stderr, "Average time is %g ms\n", (total - minTime - maxTime)/(kSamples-2));
#endif
    }
    
    CPPUNIT_TEST_SUITE( PerfTests );
    CPPUNIT_TEST( testConvert1000People );
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(PerfTests);
