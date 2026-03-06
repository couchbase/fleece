#include "Backtrace.hh"
#include <thread>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <Windows.h>

namespace fleece {
    using namespace std;

    class BacktraceSignalHandlerWin32 : public BacktraceSignalHandler {
    public:
        BacktraceSignalHandlerWin32()
            : _reporter_thread([]() {
            {
                unique_lock<mutex> lk(mtx());
                cv().wait(lk, [] { return crashed() != crash_status::running; });
            }
            if ( crashed() == crash_status::crashed ) { handle_stacktrace(); }
            {
                unique_lock<mutex> lk(mtx());
                crashed() = crash_status::ending;
            }
              cv().notify_one();
        }) {
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

         ~BacktraceSignalHandlerWin32() {
            {
                std::unique_lock<std::mutex> lk(mtx());
                crashed() = crash_status::normal_exit;
            }

            cv().notify_one();

            _reporter_thread.join();
         }
    private:
        enum class crash_status { running, crashed, normal_exit, ending };

        static crash_status& crashed() {
            static crash_status data;
            return data;
        }

        static mutex& mtx() {
            static mutex data;
            return data;
        }

        static std::string& violation_type() {
            static std::string type;
            return type;
        }

        static condition_variable& cv() {
            static condition_variable data;
            return data;
        }

        static std::shared_ptr<Backtrace>& captured_backtrace() {
            static std::shared_ptr<Backtrace> bt;
            return bt;
        }

        thread _reporter_thread;

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
            // The exception info supplies a trace from exactly where the issue was,
            // no need to skip records
            ostringstream oss;
            oss << "Exception 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << info->ExceptionRecord->ExceptionCode;
            violation_type() = oss.str();
            crash_handler_immediate(0, info->ContextRecord);
            return EXCEPTION_CONTINUE_SEARCH;
        }

        NOINLINE static void crash_handler_immediate(int skip, CONTEXT* ct = nullptr) {
            // Capture the backtrace directly in the crash handler.
            // If we have an exception context (ct != nullptr), use it to capture the stack trace
            // from exactly the point of the crash.
            if (ct != nullptr) {
                // We have an exception context - use it directly
                captured_backtrace() = Backtrace::capture(0, 32, ct);
            } else {
                // We're in a signal handler without exception context.
                // Try to capture current context, though this may not work well.
                captured_backtrace() = Backtrace::capture(skip, 32 + skip);
            }

            // Log immediately without waiting for reporter thread
            // This avoids deadlocks, especially with a debugger attached
            if ( sLogger ) {
                stringstream out;
                out << "\n\n******************** Process Crash: " << violation_type() << " ********************\n";
                captured_backtrace()->writeTo(out);
                out << "\n******************** Now terminating ********************\n";
                sLogger(out.str());
            } else {
                cerr << "\n\n******************** Process Crash: " << violation_type() << " ********************\n";
                captured_backtrace()->writeTo(cerr);
                cerr << "\n******************** Now terminating ********************\n";
            }
        }

        static void crash_handler_thread(int skip, CONTEXT* ct = nullptr) {
            // Capture the backtrace for threaded handling
            if (ct != nullptr) {
                captured_backtrace() = Backtrace::capture(0, 32, ct);
            } else {
                captured_backtrace() = Backtrace::capture(skip, 32 + skip);
            }

            {
                unique_lock<mutex> lk(mtx());
                crashed() = crash_status::crashed;
            }

            cv().notify_one();

            {
                unique_lock<mutex> lk(mtx());
                cv().wait(lk, [] { return crashed() != crash_status::crashed; });
            }
        }

        static void handle_stacktrace() {
            // Use the backtrace that was captured in the crash handler
            auto bt = captured_backtrace();
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