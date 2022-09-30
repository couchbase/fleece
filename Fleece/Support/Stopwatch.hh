//
// Stopwatch.hh
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include <algorithm>
#include <chrono>
#include <math.h>
#include <stdio.h>
#include <string>

namespace fleece {

/** A timer that can be stopped and restarted like its namesake. */
class Stopwatch {
public:
    using clock    = std::chrono::high_resolution_clock;
    using time     = clock::time_point;
    using duration = clock::duration;
    using seconds  = std::chrono::duration<double, std::ratio<1,1>>;

    Stopwatch(bool running =true) {
        if (running) start();
    }

    void start() {
        if (!_running) {
            _running = true;
            _start = clock::now();
        }
    }

    void stop() {
        if (_running) {
            _running = false;
            _total += clock::now() - _start;
        }
    }

    /** Like stop(), but returns the time taken since start() was called. */
    double lap() {
        if (!_running)
            return 0;
        _running = false;
        auto lap = clock::now() - _start;
        _total += lap;
        return std::chrono::duration_cast<seconds>(lap).count();
    }

    void reset() {
        _total = duration::zero();
        if (_running)
            _start = clock::now();
    }

    duration elapsedDuration() const {
        duration e = _total;
        if (_running)
            e += clock::now() - _start;
        return e;
    }

    double elapsed() const {
        duration e = elapsedDuration();
        return std::chrono::duration_cast<seconds>(e).count();
    }

    double elapsedMS() const    {return elapsed() * 1000.0;}

    /** Returns number of seconds since the epoch. */
    static double now() {
        auto now = std::chrono::time_point_cast<std::chrono::microseconds>(clock::now());
        return std::chrono::duration_cast<seconds>(now.time_since_epoch()).count();
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
        constexpr size_t bufSize = 50;
        char str[bufSize];
        snprintf(str, bufSize, "%.3f %s", t * scale, unit);
        return str;
    }

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
    duration _total {0};
    time _start;
    bool _running {false};
};

}
