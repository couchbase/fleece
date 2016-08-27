//
//  FleeceTests.hh
//  Fleece
//
//  Created by Jens Alfke on 11/14/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//

#pragma once

#include "Value.hh"
#include "Array.hh"
#include "Encoder.hh"
#include "Writer.hh"
#include <math.h>
#include <ostream>
#include <time.h>

using namespace fleece;

// Directory containing test files:
#define kTestFilesDir "/Couchbase/Fleece/Tests/"

// Less-obnoxious names for cppunit assertions:
#define Assert CPPUNIT_ASSERT
#define AssertEqual(ACTUAL, EXPECTED) CPPUNIT_ASSERT_EQUAL(EXPECTED, ACTUAL)


// Some operators to make slice work with AssertEqual:
std::ostream& operator<< (std::ostream& o, slice s);

std::string sliceToHex(slice);
std::string sliceToHexDump(slice, size_t width = 16);


alloc_slice readFile(const char *path);
void writeToFile(slice s, const char *path);


struct mmap_slice : public slice {
    mmap_slice(const char *path);
    ~mmap_slice();

private:
    int _fd;
    void* _mapped;
    mmap_slice(const mmap_slice&);
};


class Stopwatch {
public:
    Stopwatch()         :_start(::clock()) { }
    void reset()        {_start = clock();}
    double elapsed()    {return (clock() - _start) / (double)CLOCKS_PER_SEC;}
    double elapsedMS()  {return elapsed() * 1000.0;}
private:
    clock_t _start;
};


class Benchmark {
public:
    void start()        {_st.reset();}
    double elapsed()    {return _st.elapsed();}
    void stop()         {_times.push_back(elapsed());}

    void sort()         {std::sort(_times.begin(), _times.end());}

    double median() {
        sort();
        return _times[_times.size()/2];
    }

    double average() {
        sort();
        size_t n = _times.size(), skip = n / 10;
        double total = 0;
        for (auto t=_times.begin()+skip; t != _times.end()-skip; ++t)
            total += *t;
        return total / (n - 2*skip);
    }

    double stddev() {
        double avg = average();
        size_t n = _times.size(), skip = n / 10;
        double total = 0;
        for (auto t=_times.begin()+skip; t != _times.end()-skip; ++t)
            total += ::pow(*t - avg, 2);
        return sqrt( total / (n - 2*skip));
    }

    std::pair<double,double> range() {
        sort();
        return {_times[0], _times[_times.size()-1]};
    }

    void reset()        {_times.clear();}

    void printReport(double scale, const char *units) {
        auto r = range();
        fprintf(stderr, "Range: %g ... %g %s, Average: %g, median: %g, std dev: %.3g\n",
                r.first*scale, r.second*scale, units,
                average()*scale, median()*scale, stddev()*scale);
    }

private:
    Stopwatch _st;
    std::vector<double> _times;
};


#include <cppunit/TestCase.h>
#include <cppunit/extensions/HelperMacros.h>
