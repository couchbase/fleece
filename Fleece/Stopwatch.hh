//
//  Stopwatch.hh
//  Fleece
//
//  Created by Jens Alfke on 5/11/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include <algorithm>
#include <chrono>
#include <math.h>
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

    void reset() {
        _total = duration::zero();
        if (_running)
            _start = clock::now();
    }

    double elapsed() const {
        duration e = _total;
        if (_running)
            e += clock::now() - _start;
        return std::chrono::duration_cast<seconds>(e).count();
    }

    double elapsedMS() const    {return elapsed() * 1000.0;}

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
