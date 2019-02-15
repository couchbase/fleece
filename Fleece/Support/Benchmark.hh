//
// Benchmark.hh
//
// Copyright (c) 2016 Couchbase, Inc All rights reserved.
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

#pragma once
#include "Stopwatch.hh"
#include <vector>
#include "betterassert.hh"


class Benchmark {
public:
    void start()        {_st.reset();}
    double elapsed()    {return _st.elapsed();}
    double stop()       {double t = elapsed(); _times.push_back(t); return t;}

    bool empty() const  {return _times.empty();}
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
