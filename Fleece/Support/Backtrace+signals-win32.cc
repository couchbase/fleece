#include "Backtrace.hh"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <csignal>
#include <Windows.h>

namespace fleece {
    using namespace std;

    class BacktraceSignalHandlerWin32 : public BacktraceSignalHandler {
      public:
        BacktraceSignalHandlerWin32() {
            SetUnhandledExceptionFilter(crash_handler);
            previous_sig_handler() = signal(SIGABRT, signal_handler);
            _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
            previous_purecall_handler()          = _set_purecall_handler(&terminator);
            previous_invalid_parameter_handler() = _set_invalid_parameter_handler(&invalid_parameter_handler);
        }

        BacktraceSignalHandlerWin32(const BacktraceSignalHandlerWin32&)            = delete;
        BacktraceSignalHandlerWin32& operator=(const BacktraceSignalHandlerWin32&) = delete;
        BacktraceSignalHandlerWin32(BacktraceSignalHandlerWin32&&)                 = delete;
        BacktraceSignalHandlerWin32& operator=(BacktraceSignalHandlerWin32&&)      = delete;

      private:
        static _invalid_parameter_handler& previous_invalid_parameter_handler() {
            static _invalid_parameter_handler handler;
            return handler;
        }

        static _purecall_handler& previous_purecall_handler() {
            static _purecall_handler handler;
            return handler;
        }

        static _crt_signal_t& previous_sig_handler() {
            static _crt_signal_t handler;
            return handler;
        }

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
            signal(sig, previous_sig_handler());
            raise(sig);
            abort();
        }

        static inline void __cdecl invalid_parameter_handler(const wchar_t* a, const wchar_t* b, const wchar_t* c,
                                                             unsigned int d, uintptr_t e) {
            violation_type() = "Invalid CRT Parameter";
            crash_handler_immediate(signal_skip_recs);
            previous_invalid_parameter_handler()(a, b, c, d, e);
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
                bt = Backtrace::capture(0u, 32, ct);
            } else {
                bt = Backtrace::capture(skip, 32 + skip);
            }

            if ( sCrashStream ) {
                sCrashStream << "\n\n******************** Process Crash: " << violation_type()
                             << " ********************\n";
                bt->writeTo(sCrashStream);
                sCrashStream << "\n******************** Now terminating ********************\n";
            }

            cerr << "\n\n******************** Process Crash: " << violation_type() << " ********************\n";
            bt->writeTo(cerr);
            cerr << "\n******************** Now terminating ********************\n";
        }
    };

    // Awkwardly defined here to avoid FleeceBase getting its own copy since it also compiles Backtrace.cc
    ofstream BacktraceSignalHandler::sCrashStream;

    void BacktraceSignalHandler::setLogPath(const char* path) {
        sCrashStream = ofstream(path, ios::out | ios::trunc | ios::binary);
    }
}  // namespace fleece

static fleece::BacktraceSignalHandlerWin32 handler;
