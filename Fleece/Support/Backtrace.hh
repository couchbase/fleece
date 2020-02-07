//
// Backtrace.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include <functional>
#include <string>
#include <iostream>

namespace fleece {

    /** Captures a backtrace of the current thread, and converts it to human-readable form. */
    class Backtrace {
    public:
        explicit Backtrace(unsigned skipFrames =0);
#ifdef __clang__
        ~Backtrace();
#endif

        bool writeTo(std::ostream&);
        std::string toString();

        struct frameInfo {
            size_t pc, offset;
            const char *library, *function;
        };

        frameInfo getFrame(unsigned);

        /// Installs a C++ terminate_handler that will log a backtrace and info about any uncaught
        /// exception. By default it then calls the preexisting terminate_handler, which usually
        /// calls abort().
        ///
        /// Since the OS will not usually generate a crash report upon a SIGABORT, you can set
        /// `andRaise` to a different signal ID such as SIGILL to force a crash.
        ///
        /// Only the first call to this function has any effect; subsequent calls are ignored.
        static void installTerminateHandler(std::function<void(const std::string&)> logger);

    private:
        static constexpr size_t kMaxAddrs = 50;
        unsigned const _skip;
        void* _addrs[kMaxAddrs];
        unsigned _nAddrs;

        const char* unmangle(const char*);
        static void writeCrashLog(std::ostream&);
#ifdef __clang__
        char* _unmangled {nullptr};
        size_t _unmangledLen {0};
#endif
    };

}
