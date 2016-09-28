//
//  Benchmark.hh
//  Fleece
//
//  Created by Jens Alfke on 9/21/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include <math.h>
#include <time.h>
#include <vector>


class Stopwatch {
public:
    Stopwatch()         :_start(::clock()) { }
    void reset()        {_start = clock();}
    double elapsed()    {return (clock() - _start) / (double)CLOCKS_PER_SEC;}
    double elapsedMS()  {return elapsed() * 1000.0;}

    void printReport(const char *what, unsigned count, const char *item) {
        auto ms = elapsedMS();
        fprintf(stderr, "%s took %.3f ms for %u %ss (%.3f us/%s, or %.0f %ss/sec)\n",
                what, ms, count, item, ms/count*1000.0, item, count/ms*1000, item);
    }
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
