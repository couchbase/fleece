#include "Backtrace.hh"
#include <cstring>
#include <cstddef>
#include <iostream>
#include <memory>
#include <csignal>
#include <unistd.h>
#include <sstream>
#include <unordered_map>
#include <array>
#ifdef __APPLE__
#    include <sys/ucontext.h>
#    include "TargetConditionals.h"
#else
#    include <ucontext.h>
#endif

namespace fleece {
    using namespace std;
    using namespace signal_safe;

    namespace {
        template<typename T, size_t N>
        consteval bool allUnique(const array<T, N>& arr) {
            for (size_t i = 0; i < N; ++i)
                for (size_t j = i + 1; j < N; ++j)
                    if (arr[i] == arr[j]) return false;
            return true;
        }
    }

    class BacktraceSignalHandlerPosix : public BacktraceSignalHandler {
      public:
        static struct sigaction defaultAction() {
            struct sigaction sa{};
            sigemptyset(&sa.sa_mask);
            sa.sa_flags   = 0;
            sa.sa_handler = SIG_DFL;
            return sa;
        }

        static struct sigaction defaultActionFor(int signal) {
            const auto& actions = default_actions();
            auto        it      = actions.find(signal);
            if ( it == actions.end() ) {
                return defaultAction();
            }

            return it->second;
        }

        BacktraceSignalHandlerPosix() {
            constexpr size_t stack_size = 1024 * 1024 * 8;
            _stack_content              = make_unique<byte[]>(stack_size);
            if ( _stack_content ) {
                stack_t ss;
                ss.ss_sp    = _stack_content.get();
                ss.ss_size  = stack_size;
                ss.ss_flags = 0;
                sigaltstack(&ss, nullptr);
            }

            for ( const int signal : signals) {
                struct sigaction action{};
                // NOTE: No SA_RESETHAND since on managed platforms signals are recoverable and
                // we want our handler to keep working in the event that they are not
                action.sa_flags = SA_SIGINFO | SA_ONSTACK;
                sigfillset(&action.sa_mask);
#if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#endif
                action.sa_sigaction = &sig_handler;
#if defined(__clang__)
#    pragma clang diagnostic pop
#endif

                struct sigaction old_action;
                if ( int success = sigaction(signal, &action, &old_action); success == 0) {
                    // Only chain back if we succeeded
                    default_actions()[signal] = old_action;
                }
            }
        }

        BacktraceSignalHandlerPosix(const BacktraceSignalHandlerPosix&)            = delete;
        BacktraceSignalHandlerPosix& operator=(const BacktraceSignalHandlerPosix&) = delete;
        BacktraceSignalHandlerPosix(BacktraceSignalHandlerPosix&&)                 = delete;
        BacktraceSignalHandlerPosix& operator=(BacktraceSignalHandlerPosix&&)      = delete;

      private:
        unique_ptr<byte[]> _stack_content;

#ifdef __APPLE__
        static constexpr array<int, 10> signals = {
#else
        static constexpr array<int, 9> signals = {
#endif
                SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGQUIT, SIGSEGV, SIGSYS, SIGTRAP, SIGXFSZ,
#if defined(__APPLE__)
                SIGEMT,
#endif
        };

        static_assert(allUnique(signals), "Signals array contains duplicate entries");

        static unordered_map<int, struct sigaction>& default_actions() {
            static unordered_map<int, struct sigaction> da;
            return da;
        }

        NOINLINE static void crash_handler_immediate(siginfo_t* info, void* context) {
            void*       buffer[50];
            int         n    = Backtrace::raw_capture(buffer, 50, context);
            write_to_and_stderr(sLogFD, "\n\n******************** Signal caught: ", 38);
            write_long(info->si_signo, sLogFD);

            write_to_and_stderr(sLogFD, " Timestamp: ", 12);
            write_long(time(nullptr), sLogFD);

            write_to_and_stderr(sLogFD, " *******************\n", 21);
            Backtrace::writeTo(buffer + 3, n - 3, sLogFD);
            if ( sLogFD != -1 ) {
                fsync(sLogFD);
            }
        }

        static void chain_to_previous(int signo, siginfo_t* info, void* context) {
            const auto& prev = defaultActionFor(signo);
            if ((prev.sa_flags & SA_SIGINFO) && prev.sa_sigaction && prev.sa_sigaction != &sig_handler) {
                // Original fault context: the runtime can recover (and modify *context,
                // which our return then resumes into) or die inside this call.
                prev.sa_sigaction(signo, info, context);
            } else if (prev.sa_handler && prev.sa_handler != SIG_IGN
                 && prev.sa_handler != SIG_DFL) {
                // SIG_IGN and SIG_DFL are not actual functions, don't try to call them
                prev.sa_handler(signo);
            } else if (prev.sa_handler == SIG_DFL) {
                sigaction(signo, &prev, nullptr);   // restore default and let it act
                raise(signo);                       // fatal default terminates here
            }
            //SIG_IGN means ignore, so do just that
        }

        static void sig_handler(int signo, siginfo_t* info, void* context) {
            crash_handler_immediate(info, context);
            chain_to_previous(signo, info, context);
            write_to_and_stderr(sLogFD,
                "\n********** Signal handled; execution continuing **********\n", 60);
        }
    };

    // Awkwardly defined here to avoid FleeceBase getting its own copy since it also compiles Backtrace.cc
    volatile sig_atomic_t BacktraceSignalHandler::sLogFD = -1;

    void BacktraceSignalHandler::setLogPath(const char* path) {
        if ( sLogFD != -1 ) { close(sLogFD); }

        sLogFD = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }
}  // namespace fleece

#if !TARGET_OS_IOS
[[maybe_unused]]
static fleece::BacktraceSignalHandlerPosix handler;
#endif
