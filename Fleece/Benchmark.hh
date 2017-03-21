//
//  Benchmark.hh
//  Fleece
//
//  Created by Jens Alfke on 9/21/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include <math.h>
#include <string>
#include <time.h>
#include <algorithm>
#include <vector>


/** A high precision time unit based on POSIX's struct timespec. */
class Timespec {
public:
    Timespec()                  :_spec{0, 0} { }
    Timespec(struct timespec s) :_spec{s}    { }

    static Timespec now() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
        return Timespec(ts);
    }

    Timespec age() const {
        return now() - *this;
    }

    operator double() const {
        return _spec.tv_sec + _spec.tv_nsec / 1.0e9;
    }

    Timespec& operator= (double secs) {
        _spec.tv_sec = (long)floor(secs);
        _spec.tv_nsec = (long)((secs - floor(secs)) * 1.0e9);
        return *this;
    }

    Timespec operator- (const Timespec &other) const {
        return {_spec.tv_sec - other._spec.tv_sec, _spec.tv_nsec - other._spec.tv_nsec};
    }

    Timespec operator+ (const Timespec &other) const {
        return {_spec.tv_sec + other._spec.tv_sec, _spec.tv_nsec + other._spec.tv_nsec};
    }

    Timespec& operator+= (const Timespec &other) {
        _spec.tv_sec  += other._spec.tv_sec;
        _spec.tv_nsec += other._spec.tv_nsec;
        return *this;
    }

private:
    Timespec(long secs, long nsec) :_spec{secs, nsec}  { }

    struct timespec _spec;
};


/** A timer that can be stopped and restarted like its namesake. */
class Stopwatch {
public:
    Stopwatch(bool running =true) {
        if (running) start();
    }

    void start() {
        if (!_running) {
            _running = true;
            _start = Timespec::now();
        }
    }

    void stop() {
        if (_running) {
            _running = false;
            _total += _start.age();
        }
    }

    void reset() {
        _total = 0.0;
        if (_running)
            _start = Timespec::now();
    }

    Timespec elapsed() const {
        Timespec e = _total;
        if (_running)
            e += _start.age();
        return e;
    }

    double elapsedMS() const    {return elapsed() * 1000.0;}

    void printReport(const char *what, unsigned count, const char *item) const {
        auto ms = elapsedMS();
#ifdef NDEBUG
        fprintf(stderr, "%s took %.3f ms for %u %ss (%.3f us/%s, or %.0f %ss/sec)\n",
                what, ms, count, item, ms/count*1000.0, item, count/ms*1000, item);
#else
        fprintf(stderr, "%s; %u %ss (took %.3f ms, but this is UNOPTIMIZED CODE)\n",
                what, count, item, ms);
#endif
    }
private:
    Timespec _total, _start;
    bool _running {false};
};


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

    static double timeScale(double t, const char* &unit) {
        static const char* kTimeScales[] = {"sec", "ms", "us", "ns"};
        double scale = 1.0;
        for (unsigned i = 0; i < sizeof(kTimeScales)/sizeof(char*); ++i) {
            unit = kTimeScales[i];
            if (t*scale >= 1.0)
                break;
            scale *= 1000;
        }
        return scale;
    }

    static std::string formatTime(double t) {
        const char *unit;
        double scale = timeScale(t, unit);
        char str[50];
        sprintf(str, "%.3f %s", t * scale, unit);
        return str;
    }

private:
    Stopwatch _st;
    std::vector<double> _times;
};
