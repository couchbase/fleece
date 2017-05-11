//
//  Benchmark.hh
//  Fleece
//
//  Created by Jens Alfke on 9/21/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include "Stopwatch.hh"
#include <vector>


class Benchmark {
public:
    void start()        {_st.reset();}
    double elapsed()    {return _st.elapsed();}
    double stop()       {double t = elapsed(); _times.push_back(t); return t;}

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

    void printReport(double scale =1.0, const char *items =nullptr) {
        auto r = range();

        std::string scaleName;
        const char* kTimeScales[] = {"sec", "ms", "us", "ns"};
        double avg = average();
        for (unsigned i = 0; i < sizeof(kTimeScales)/sizeof(char*); ++i) {
            scaleName = kTimeScales[i];
            if (avg*scale >= 1.0)
                break;
            scale *= 1000;
        }

        if (items)
            scaleName += std::string("/") + std::string(items);

        fprintf(stderr, "Range: %.3f ... %.3f %s, Average: %.3f, median: %.3f, std dev: %.3g\n",
                r.first*scale, r.second*scale, scaleName.c_str(),
                average()*scale, median()*scale, stddev()*scale);
    }

private:
    fleece::Stopwatch _st;
    std::vector<double> _times;
};
