#include "Backtrace.hh"
#include <cstring>
#include <iostream>
#include <memory>
#include <signal.h>
#include <sstream>
#include <ucontext.h>

namespace fleece {
    using namespace std;

    class BacktraceSignalHandlerPosix : public BacktraceSignalHandler {
      public:
        static vector<int> signals() {
            return {
                    SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGIOT, SIGQUIT, SIGSEGV, SIGSYS, SIGTRAP, SIGXCPU, SIGXFSZ,
#if defined(__APPLE__)
                    SIGEMT,
#endif
            };
        }

        BacktraceSignalHandlerPosix() {
            constexpr size_t stack_size = 1024 * 1024 * 8;
            _stack_content              = malloc(stack_size);
            if ( _stack_content ) {
                stack_t ss;
                ss.ss_sp    = _stack_content;
                ss.ss_size  = stack_size;
                ss.ss_flags = 0;
                sigaltstack(&ss, nullptr);
            }

            auto signal_vector = signals();
            for ( size_t i = 0; i < signal_vector.size(); ++i ) {
                struct sigaction action;
                memset(&action, 0, sizeof(action));
                action.sa_flags = static_cast<int>(SA_SIGINFO | SA_ONSTACK | SA_NODEFER | SA_RESETHAND);
                sigfillset(&action.sa_mask);
                sigdelset(&action.sa_mask, signal_vector[i]);
#if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#endif
                action.sa_sigaction = &sig_handler;
#if defined(__clang__)
#    pragma clang diagnostic pop
#endif

                sigaction(signal_vector[i], &action, nullptr);
            }
        }

        BacktraceSignalHandlerPosix(const BacktraceSignalHandlerPosix&)            = delete;
        BacktraceSignalHandlerPosix& operator=(const BacktraceSignalHandlerPosix&) = delete;
        BacktraceSignalHandlerPosix(BacktraceSignalHandlerPosix&&)                 = delete;
        BacktraceSignalHandlerPosix& operator=(BacktraceSignalHandlerPosix&&)      = delete;

      private:
        void* _stack_content;

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

        NOINLINE static void crash_handler_immediate(void* error_addr) {
            shared_ptr<Backtrace> bt;
            if ( error_addr ) {
                bt = Backtrace::capture(error_addr);
            } else {
                bt = Backtrace::capture();
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

        [[noreturn]] static void sig_handler(int signo, siginfo_t* info, void* context) {
            const char* name = strsignal(signo);
            violation_type() = name ? name : "Signal " + to_string(signo);

            crash_handler_immediate(extract_pc(context));

            raise(info->si_signo);

            _exit(EXIT_FAILURE);
        }
    };
}  // namespace fleece

fleece::BacktraceSignalHandler& getSignalHandler() {
    static fleece::BacktraceSignalHandlerPosix handler;
    return handler;
}
