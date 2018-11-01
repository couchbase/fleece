//
// Backtrace.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include <string>
#include <iostream>

namespace fleece {

    /** Captures a backtrace of the current thread, and converts it to human-readable form. */
    class Backtrace {
    public:
        Backtrace(unsigned skipFrames =0);
        ~Backtrace();

        bool writeTo(std::ostream&);
        std::string toString();

        struct frameInfo {
            size_t pc, offset;
            const char *library, *function;
        };

        frameInfo getFrame(unsigned);

    private:
        static constexpr size_t kMaxAddrs = 50;
        unsigned const _skip;
        void* _addrs[kMaxAddrs];
        unsigned _nAddrs;

#ifndef _MSC_VER
        const char* unmangle(const char*);
        char* _unmangled {nullptr};
        size_t _unmangledLen {0};
#endif
    };

}
