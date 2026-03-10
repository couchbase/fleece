#include "Backtrace.hh"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <Windows.h>

namespace fleece {
    using namespace std;

    class BacktraceSignalHandlerWin32 : public BacktraceSignalHandler {
      public:
        BacktraceSignalHandlerWin32() {
            SetUnhandledExceptionFilter(crash_handler);
            signal(SIGABRT, signal_handler);
            _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
            _set_purecall_handler(&terminator);
            _set_invalid_parameter_handler(&invalid_parameter_handler);
        }

        BacktraceSignalHandlerWin32(const BacktraceSignalHandlerWin32&)            = delete;
        BacktraceSignalHandlerWin32& operator=(const BacktraceSignalHandlerWin32&) = delete;
        BacktraceSignalHandlerWin32(BacktraceSignalHandlerWin32&&)                 = delete;
        BacktraceSignalHandlerWin32& operator=(BacktraceSignalHandlerWin32&&)      = delete;

      private:
        static string& violation_type() {
            static string type;
            return type;
        }

        static const constexpr int signal_skip_recs =
#ifdef __clang__
                // With clang, RtlCaptureContext also captures the stack frame of the
                // current function Below that, there are 3 internal Windows functions
                4
#else
                // With MSVC cl, RtlCaptureContext misses the stack frame of the current
                // function The first entries during StackWalk are the 3 internal Windows
                // functions
                3
#endif
                ;

        static inline void terminator() {
            violation_type() = "Pure Virtual Function Call";
            crash_handler_immediate(signal_skip_recs);
            abort();
        }

        static inline void signal_handler(int sig) {
            violation_type() = (sig == SIGABRT) ? "Abort Signal (SIGABRT)" : "Signal";
            crash_handler_immediate(signal_skip_recs);
            abort();
        }

        static inline void __cdecl invalid_parameter_handler(const wchar_t*, const wchar_t*, const wchar_t*,
                                                             unsigned int, uintptr_t) {
            violation_type() = "Invalid CRT Parameter";
            crash_handler_immediate(signal_skip_recs);
            abort();
        }

        NOINLINE static LONG WINAPI crash_handler(EXCEPTION_POINTERS* info) {
            ostringstream oss;
            oss << "Exception 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8)
                << info->ExceptionRecord->ExceptionCode;
            violation_type() = oss.str();
            crash_handler_immediate(0, info->ContextRecord);
            return EXCEPTION_CONTINUE_SEARCH;
        }

        NOINLINE static void crash_handler_immediate(int skip, CONTEXT* ct = nullptr) {
            shared_ptr<Backtrace> bt;
            if ( ct != nullptr ) {
                bt = Backtrace::capture(0, 32, ct);
            } else {
                bt = Backtrace::capture(skip, 32 + skip);
            }

            if ( sLogger ) {
                stringstream out;
                out << "\n\n******************** Process Crash: " << violation_type() << " ********************\n";
                bt->writeTo(out);
                out << "\n******************** Now terminating ********************\n";
                sLogger(out.str());
            } else {
                cerr << "\n\n******************** Process Crash: " << violation_type() << " ********************\n";
                bt->writeTo(cerr);
                cerr << "\n******************** Now terminating ********************\n";
            }
        }
    };
}  // namespace fleece

fleece::BacktraceSignalHandler& getSignalHandler() {
    static fleece::BacktraceSignalHandlerWin32 handler;
    return handler;
}
