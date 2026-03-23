//
// Backtrace.hh
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "fleece/PlatformCompat.hh"
#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>
#include <csignal>
#include <fstream>
#ifndef _WIN32
#    include <unistd.h>
#    include <sys/fcntl.h>

namespace signal_safe {
    void write_long(long long value, int fd);
    void write_ulong(unsigned long long value, int fd);
    void write_hex_offset(size_t value, int fd);
    void write_to_and_stderr(int fd, const char* str, size_t len = 0);
}  // namespace signal_safe
#endif

namespace fleece {
    class BacktraceSignalHandler {
      public:
        static void setLogPath(const char* path);

      protected:
#ifdef _WIN32
        static std::ofstream sCrashStream;
#else
        static volatile sig_atomic_t sLogFD;
#endif
    };

    /** Captures a backtrace of the current thread, and can convert it to human-readable form. */
    class Backtrace {
      public:
        /// Captures a backtrace and returns a shared pointer to the instance.
        [[nodiscard]] static std::shared_ptr<Backtrace> capture(unsigned skipFrames = 0, unsigned maxFrames = 50,
                                                                void* context = nullptr);

        static int raw_capture(void** buffer, int max, void* context = nullptr);

        /// Captures a backtrace, unless maxFrames is zero.
        /// @param skipFrames  Number of frames to skip at top of stack
        /// @param maxFrames  Maximum number of frames to capture
        /// @param context     If non-null, a platform-specific context to capture from
        /// @param from       If non-null, the address to start from instead of the current frame
        explicit Backtrace(unsigned skipFrames = 0, unsigned maxFrames = 50, void* context = nullptr);

        ~Backtrace();

        /// Removes frames from the top of the stack.
        void skip(unsigned nFrames);

        /// Writes the human-readable backtrace to a stream.
        bool writeTo(std::ostream&) const;

        static void writeTo(void** addresses, int size, int fd);

        /// Returns the human-readable backtrace.
        std::string toString() const;

        // Direct access to stack frames:

        struct frameInfo {
            const void* pc;           ///< Program counter
            size_t      offset;       ///< Byte offset of pc in function
            const char* function;     ///< Name of (nearest) known function
            const char* library;      ///< Name of dynamic library containing the function
            size_t      imageOffset;  ///< Byte offset of pc in the library/image
            const char* file;         ///< Source file name, if available
            int         line;         ///< Source line number, if available
        };

        /// The number of stack frames captured.
        unsigned size() const { return (unsigned)_addrs.size(); }

      private:
        void _capture(unsigned skipFrames = 0, unsigned maxFrames = 50, void* context = nullptr);
#ifndef _WIN32
        const char*      getSymbol(unsigned i) const;
        static void      writeCrashLog(std::ostream&);
        static void      handleTerminate(const std::function<void(const std::string&)>& logger);
        frameInfo        getFrame(unsigned) const;
        static frameInfo getFrame(const void* pc, bool stack_top);
        static char*     unmangle(const char* function NONNULL);
#endif

        std::vector<void*> _addrs;  // Array of PCs in backtrace, top first
        mutable char**     _symbols{nullptr};
    };

    /// Attempts to return the unmangled name of the type. (If it fails, returns the mangled name.)
    std::string Unmangle(const std::type_info&);

    /// Attempts to unmangle a name. (If it fails, returns the input string unaltered.)
    std::string Unmangle(const char* name NONNULL);

    /// Returns the name of the function at the given address, or an empty string if none.
    /// The name will be unmangled, if possible.
    std::string FunctionName(const void* pc);

}  // namespace fleece
