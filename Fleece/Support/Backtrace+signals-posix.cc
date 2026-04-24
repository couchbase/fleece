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
            if ( !default_actions().contains(signal) ) {
                return defaultAction();
            }

            return default_actions()[signal];
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
                action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER | SA_RESETHAND;
                sigfillset(&action.sa_mask);
                sigdelset(&action.sa_mask, signal);
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
        static constexpr array<int, 11> signals = {
#else
        static constexpr array<int, 10> signals = {
#endif
                SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGQUIT, SIGSEGV, SIGSYS, SIGTRAP, SIGXCPU, SIGXFSZ,
#if defined(__APPLE__)
                SIGEMT,
#endif
        };

        static_assert(allUnique(signals), "Signals array contains duplicate entries");

        static unordered_map<int, struct sigaction>& default_actions() {
            static unordered_map<int, struct sigaction> da;
            return da;
        }

        static string& violation_type() {
            static string type;
            return type;
        }

        static void* extract_pc(void* context) {
            auto* uctx = static_cast<ucontext_t*>(context);
#ifdef REG_RIP  // x86_64 Linux
            return reinterpret_cast<void*>(uctx->uc_mcontext.gregs[REG_RIP]);
#elif defined(__aarch64__)
#    if defined(__APPLE__)
            return reinterpret_cast<void*>(uctx->uc_mcontext->__ss.__pc);
#    else
            return reinterpret_cast<void*>(uctx->uc_mcontext.pc);
#    endif
#elif defined(__APPLE__) && defined(__x86_64__)
            return reinterpret_cast<void*>(uctx->uc_mcontext->__ss.__rip);
#else
#    error "Unsupported architecture"
#endif
        }

        NOINLINE static void crash_handler_immediate(siginfo_t* info, void* context) {
            void*       buffer[50];
            int         n    = Backtrace::raw_capture(buffer, 50, context);
            const char* name = strsignal(info->si_signo);
            write_to_and_stderr(sLogFD, "\n\n******************** Process Crash: ", 38);
            if ( name ) {
                write_to_and_stderr(sLogFD, name);
            } else {
                write_to_and_stderr(sLogFD, "Signal: ", 8);
                write_long(info->si_signo, sLogFD);
            }

            write_to_and_stderr(sLogFD, " Timestamp: ", 12);
            char timestamp[20];
            snprintf(timestamp, 20, "%lld", static_cast<long long>(time(nullptr)));
            write_long(time(nullptr), sLogFD);

            write_to_and_stderr(sLogFD, " *******************\n", 21);
            Backtrace::writeTo(buffer + 3, n - 3, sLogFD);
            write_to_and_stderr(sLogFD, "\n******************** Now terminating ********************\n", 59);
            if ( sLogFD != -1 ) {
                fsync(sLogFD);
                close(sLogFD);
            }
        }

        [[noreturn]] static void sig_handler(int signo, siginfo_t* info, void* context) {
            crash_handler_immediate(info, context);

            auto default_action = BacktraceSignalHandlerPosix::defaultActionFor(signo);
            sigaction(signo, &default_action, nullptr);
            raise(signo);

            _exit(128 + signo);
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
