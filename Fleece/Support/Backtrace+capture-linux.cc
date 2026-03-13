#if !defined(__linux__) && !defined(__ANDROID__)
#    error "This implementation is meant for Linux and Android only
#endif

#include "Backtrace.hh"
#include <dlfcn.h>   // dladdr()
#include <unwind.h>  // part of compiler runtime, no extra link needed
#include <backtrace.h>
#include <mutex>
#include <cstring>

namespace fleece {
    using namespace std;

    struct BacktraceState {
        void** current;
        void** end;
    };

    static _Unwind_Reason_Code unwindCallback(struct _Unwind_Context* context, void* arg) {
        BacktraceState* state = static_cast<BacktraceState*>(arg);
        uintptr_t       pc    = _Unwind_GetIP(context);
        if ( pc ) {
            if ( state->current == state->end ) return _URC_END_OF_STACK;
            *state->current++ = reinterpret_cast<void*>(pc);
        }
        return _URC_NO_REASON;
    }

    Backtrace::frameInfo getFrame(const void* addr, bool stack_top) {
        frameInfo frame = {};
        frame.pc        = addr;

        // dladdr gives us the library name regardless of symbol visibility,
        // and a fallback saddr/sname for exported symbols.
        Dl_info dlInfo = {};
        dladdr(frame.pc, &dlInfo);
        if ( dlInfo.dli_fname ) {
            frame.library = dlInfo.dli_fname;
            if ( const char* slash = strrchr(frame.library, '/') ) frame.library = slash + 1;
        }

        uintptr_t pc = reinterpret_cast<uintptr_t>(frame.pc);
        if ( !stack_top ) pc -= 1;  // return address → call site
        if ( dlInfo.dli_fbase ) frame.imageOffset = pc - reinterpret_cast<uintptr_t>(dlInfo.dli_fbase);
        auto resolved  = backtraceResolve(pc);
        frame.function = resolved.function;
        frame.file     = resolved.file;
        frame.line     = resolved.line;
        // Prefer symval from libbacktrace (works for hidden-visibility symbols);
        // fall back to dladdr's saddr for exported symbols.
        if ( resolved.symval ) frame.offset = reinterpret_cast<uintptr_t>(frame.pc) - resolved.symval;
        else if ( dlInfo.dli_saddr )
            frame.offset = (size_t)frame.pc - (size_t)dlInfo.dli_saddr;

        return frame;
    }

    int Backtrace::raw_capture(void** buffer, int max, void* context) {
        BacktraceState state = {buffer, buffer + max};
        _Unwind_Backtrace(unwindCallback, &state);
        return int(state.current - buffer);
    }

    static backtrace_state* getBacktraceState() {
        static std::once_flag   once;
        static backtrace_state* state;
        std::call_once(once, [] { state = backtrace_create_state(nullptr, /*threaded=*/1, nullptr, nullptr); });
        return state;
    }

    struct ResolvedInfo {
        const char* function = nullptr;
        const char* file     = nullptr;
        int         line     = 0;
        uintptr_t   symval   = 0;  // function start address, for offset calculation
    };

    static ResolvedInfo backtraceResolve(uintptr_t pc) {
        ResolvedInfo info;
        backtrace_pcinfo(
                getBacktraceState(), pc,
                [](void* data, uintptr_t, const char* file, int line, const char* fn) -> int {
                    auto* r = static_cast<ResolvedInfo*>(data);
                    if ( fn && !r->function ) r->function = fn;
                    if ( file && !r->file ) {
                        r->file = file;
                        r->line = line;
                    }
                    return 0;
                },
                nullptr, &info);

        // Always call syminfo: provides symval (function start) for offset, and function name fallback
        backtrace_syminfo(
                getBacktraceState(), pc,
                [](void* data, uintptr_t, const char* sym, uintptr_t symval, uintptr_t) {
                    auto* r = static_cast<ResolvedInfo*>(data);
                    if ( !r->function && sym ) r->function = sym;
                    if ( symval ) r->symval = symval;
                },
                nullptr, &info);
        return info;
    }

    const char* Backtrace::getSymbol(unsigned i) const {
        // delegate to getFrame which already uses libbacktrace
        static thread_local string tName;
        auto                       frame = getFrame(i);
        if ( frame.function ) {
            tName = frame.function;
            return tName.c_str();
        }

        return nullptr;
    }

    string FunctionName(const void* pc) {
#if !defined(__ANDROID__)
        uintptr_t addr = reinterpret_cast<uintptr_t>(pc) - 1;
        if ( const char* name = backtraceResolve(addr).function ) return Unmangle(name);
        return {};
#else
        Dl_info info = {};
        dladdr(pc, &info);
        if ( info.dli_sname ) return Unmangle(info.dli_sname);
        return {};
#endif
    }
}  // namespace fleece
