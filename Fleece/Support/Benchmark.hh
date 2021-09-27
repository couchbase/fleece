//
// Benchmark.hh
//
// Copyright 2016-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "Stopwatch.hh"
#include <algorithm>
#include <utility>
#include <vector>
#include "betterassert.hh"


class Benchmark {
public:
    void start()        {_st.reset();}
    double elapsed() const FLPURE    {return _st.elapsed();}
    double stop()       {double t = elapsed(); _times.push_back(t); return t;}

    bool empty() const FLPURE  {return _times.empty();}
    void sort()         {assert(!empty()); std::sort(_times.begin(), _times.end());}

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
            if (i > 0)
                scale *= 1000;
            scaleName = kTimeScales[i];
            if (avg*scale >= 1.0)
                break;
        }

        if (items)
            scaleName += std::string("/") + std::string(items);

        fprintf(stderr, "Median %7.3f %s; mean %7.3f; std dev %5.3g; range (%7.3f ... %7.3f)\n",
                median()*scale, scaleName.c_str(), average()*scale, stddev()*scale,
                r.first*scale, r.second*scale);
    }

private:
    fleece::Stopwatch _st;
    std::vector<double> _times;
};
