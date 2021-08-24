//
// Backtrace.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "PlatformCompat.hh"
#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>

namespace fleece {

    /** Captures a backtrace of the current thread, and can convert it to human-readable form. */
    class Backtrace {
    public:
        /// Captures a backtrace and returns a shared pointer to the instance.
        static std::shared_ptr<Backtrace> capture(unsigned skipFrames =0, unsigned maxFrames =50);

        /// Captures a backtrace, unless maxFrames is zero.
        /// @param skipFrames  Number of frames to skip at top of stack
        /// @param maxFrames  Maximum number of frames to capture
        explicit Backtrace(unsigned skipFrames =0, unsigned maxFrames =50);

        /// Removes frames from the top of the stack.
        void skip(unsigned nFrames);

        /// Writes the human-readable backtrace to a stream.
        bool writeTo(std::ostream&) const;

        /// Returns the human-readable backtrace.
        std::string toString() const;

        // Direct access to stack frames:

        struct frameInfo {
            const void* pc;         ///< Program counter
            size_t offset;          ///< Byte offset of pc in function
            const char *function;   ///< Name of (nearest) known function
            const char *library;    ///< Name of dynamic library containing the function
        };

        /// The number of stack frames captured.
        unsigned size() const                   {return (unsigned)_addrs.size();}

        /// Returns info about a stack frame. 0 is the top.
        frameInfo getFrame(unsigned) const;
        frameInfo operator[] (unsigned i)       {return getFrame(i);}

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
        void _capture(unsigned skipFrames =0, unsigned maxFrames =50);
        char* printFrame(unsigned i) const;
        static void writeCrashLog(std::ostream&);

        std::vector<void*> _addrs;          // Array of PCs in backtrace, top first
    };


    /// Attempts to return the unmangled name of the type. (If it fails, returns the mangled name.)
    std::string Unmangle(const std::type_info&);

    /// Attempts to unmangle a name. (If it fails, returns the input string unaltered.)
    std::string Unmangle(const char *name NONNULL);

    /// Returns the name of the function at the given address, or an empty string if none.
    /// The name will be unmangled, if possible.
    std::string FunctionName(const void *pc);

}
