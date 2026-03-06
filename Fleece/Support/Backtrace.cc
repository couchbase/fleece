//
// Backtrace.cc
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "Backtrace.hh"
#include <csignal>
#include <exception>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string.h>
#include <algorithm>
#include "betterassert.hh"

fleece::BacktraceSignalHandler& getSignalHandler();
std::function<void(const std::string&)> fleece::BacktraceSignalHandler::sLogger;

#ifndef _MSC_VER
#    include <dlfcn.h>   // dladdr()
#    include <cxxabi.h>  // abi::__cxa_demangle()

#    if defined(__ANDROID__) || (defined(__linux__) && defined(HAVE_LIBBACKTRACE))
#        include <unwind.h>  // part of compiler runtime, no extra link needed
#        ifdef HAVE_LIBBACKTRACE
#            include <backtrace.h>
#        endif

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

static int backtrace(void** buffer, int max, void* context = nullptr) {
    BacktraceState state = {buffer, buffer + max};
    _Unwind_Backtrace(unwindCallback, &state);
    return int(state.current - buffer);
}

#        ifdef HAVE_LIBBACKTRACE
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
#        endif  // HAVE_LIBBACKTRACE

#    else
#        define HAVE_EXECINFO
#        include <execinfo.h>
#    endif


namespace fleece {
    using namespace std;

    static char* unmangle(const char* function) {
        int    status;
        size_t unmangledLen;
        char*  unmangled = abi::__cxa_demangle(function, nullptr, &unmangledLen, &status);
        if ( unmangled && status == 0 ) return unmangled;
        free(unmangled);
        return (char*)function;
    }

    Backtrace::frameInfo Backtrace::getFrame(unsigned i) const {
        precondition(i < _addrs.size());
        frameInfo frame = {};
        frame.pc        = _addrs[i];

        // dladdr gives us the library name regardless of symbol visibility,
        // and a fallback saddr/sname for exported symbols.
        Dl_info dlInfo = {};
        dladdr(frame.pc, &dlInfo);
        if ( dlInfo.dli_fname ) {
            frame.library = dlInfo.dli_fname;
            if ( const char* slash = strrchr(frame.library, '/') ) frame.library = slash + 1;
        }
#    if defined(HAVE_LIBBACKTRACE)
        uintptr_t pc = reinterpret_cast<uintptr_t>(frame.pc);
        if ( i > 0 ) pc -= 1;  // return address → call site
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
#    else
        frame.function = dlInfo.dli_sname;  // exported symbols only
        if ( dlInfo.dli_saddr ) frame.offset = (size_t)frame.pc - (size_t)dlInfo.dli_saddr;
        if ( dlInfo.dli_fbase ) {
            uintptr_t pc = reinterpret_cast<uintptr_t>(frame.pc);
            if ( i > 0 ) pc -= 1;
            frame.imageOffset = pc - reinterpret_cast<uintptr_t>(dlInfo.dli_fbase);
        }
#    endif
        return frame;
    }

    const char* Backtrace::getSymbol(unsigned i) const {
#    if defined(HAVE_LIBBACKTRACE)
        // delegate to getFrame which already uses libbacktrace
        static thread_local string tName;
        auto                       frame = getFrame(i);
        if ( frame.function ) {
            tName = frame.function;
            return tName.c_str();
        }
        return nullptr;
#    elif defined(HAVE_EXECINFO)
        precondition(i < _addrs.size());
        if ( !_symbols ) _symbols = backtrace_symbols(_addrs.data(), int(_addrs.size()));
        if ( _symbols ) {
            const char* s = _symbols[i];
#        if __APPLE__
            while ( *s && isdigit(*s) ) ++s;
            while ( *s && isspace(*s) ) ++s;
#        else
            const char* slash = strrchr(s, '/');
            if ( slash ) s = slash + 1;
#        endif
            return s;
        }
#    endif
        return nullptr;
    }

    // If any of these strings occur in a backtrace, suppress further frames.
    static constexpr const char* kTerminalFunctions[] = {
            "_C_A_T_C_H____T_E_S_T_",
            "Catch::TestInvokerAsFunction::invoke() const",
            "litecore::actor::Scheduler::task(unsigned)",
            "litecore::actor::GCDMailbox::safelyCall",
    };

    static constexpr struct {
        const char *old, *nuu;
    } kAbbreviations[] = {
            {"(anonymous namespace)", "(anon)"},
            {"std::__1::", "std::"},
            {"std::basic_string<char, std::char_traits<char>, std::allocator<char> >", "std::string"},
    };

    static void replace(std::string& str, string_view oldStr, string_view newStr) {
        string::size_type pos = 0;
        while ( string::npos != (pos = str.find(oldStr, pos)) ) {
            str.replace(pos, oldStr.size(), newStr);
            pos += newStr.size();
        }
    }

    bool Backtrace::writeTo(ostream& out) const {
        for ( unsigned i = 0; i < unsigned(_addrs.size()); ++i ) {
            if ( i > 0 ) out << '\n';
            out << '\t';
            char*       cstr  = nullptr;
            auto        frame = getFrame(i);
            int         len;
            bool        stop = false;
            const char* lib  = frame.library ? frame.library : "?";
            string      name = frame.function ? Unmangle(frame.function) : string();
            if ( !name.empty() ) {
                for ( auto fn : kTerminalFunctions )
                    if ( name.find(fn) != string::npos ) stop = true;
                for ( auto& abbrev : kAbbreviations ) replace(name, abbrev.old, abbrev.nuu);
            }
            if ( frame.file ) {
                const char* fileBase = strrchr(frame.file, '/');
                fileBase             = fileBase ? fileBase + 1 : frame.file;
                len = asprintf(&cstr, "%2u  %-25s %s + %zu  (%s:%d)", i, lib, name.empty() ? "?" : name.c_str(),
                               frame.offset, fileBase, frame.line);
            } else {
                len = asprintf(&cstr, "%2u  %-25s %s + %zu  [0x%zx]", i, lib, name.empty() ? "?" : name.c_str(),
                               frame.offset, frame.imageOffset);
            }

            if ( len < 0 ) return false;

            if ( len > 0 ) {
                out.write(cstr, size_t(len));
                free(cstr);
            }

            if ( stop ) {
                out << "\n\t ... (" << (_addrs.size() - i - 1) << " more suppressed) ...";
                break;
            }
        }
        return true;
    }

    std::string FunctionName(const void* pc) {
#    if defined(__linux__) && !defined(__ANDROID__) && defined(HAVE_LIBBACKTRACE)
        uintptr_t addr = reinterpret_cast<uintptr_t>(pc) - 1;
        if ( const char* name = backtraceResolve(addr).function ) return Unmangle(name);
        return {};
#    else
        Dl_info info = {};
        dladdr(pc, &info);
        if ( info.dli_sname ) return Unmangle(info.dli_sname);
        return {};
#    endif
    }

}  // namespace fleece


#else  // _MSC_VER
#    pragma mark - WINDOWS IMPLEMENTATION:

#    pragma comment(lib, "Dbghelp.lib")
#    include <Windows.h>
#    include <Dbghelp.h>
#    include "asprintf.h"
#    include <sstream>
#    include <iomanip>
using namespace std;

namespace fleece {

    static void initialize_symbols() {
        static bool initialized = false;
        if (!initialized) {
            auto process = GetCurrentProcess();
            SymInitialize(process, nullptr, TRUE);
            DWORD symOptions = SymGetOptions();
            symOptions |= SYMOPT_LOAD_LINES | SYMOPT_UNDNAME;
            SymSetOptions(symOptions);
            initialized = true;
        }
    }

    static int backtrace(void** buffer, int max, void* context) {
        CONTEXT localCtx;
        CONTEXT* myCtx;
        if (context) {
            memcpy(&localCtx, context, sizeof(CONTEXT));
            myCtx = &localCtx;
        } else {
            RtlCaptureContext(&localCtx);
            myCtx = &localCtx;
        }

        if ( max == 0 ) return 0;

        // Initialize symbol handler for StackWalk64
        initialize_symbols();

        // StackWalk64 needs a valid thread handle. If we have a captured context,
        // we use GetCurrentThread() since we're walking while in a valid thread context.
        HANDLE thread = GetCurrentThread();
        auto process = GetCurrentProcess();
        STACKFRAME64 s;
        memset(&s, 0, sizeof(STACKFRAME64));

        s.AddrStack.Mode = AddrModeFlat;
        s.AddrFrame.Mode = AddrModeFlat;
        s.AddrPC.Mode    = AddrModeFlat;
#    if defined(_M_X64)
        s.AddrPC.Offset = myCtx->Rip;
        s.AddrStack.Offset = myCtx->Rsp;
        s.AddrFrame.Offset = myCtx->Rbp;
        auto machine_type  = IMAGE_FILE_MACHINE_AMD64;
#    elif defined(_M_ARM64)
        s.AddrPC.Offset    = myCtx->Pc;
        s.AddrStack.Offset = myCtx->Sp;
        s.AddrFrame.Offset = myCtx->R11;
        auto machine_type  = IMAGE_FILE_MACHINE_ARM64;
#    else
#        error Unsupported architecture
#    endif

        auto size = 0;
        for (int i = 0; i < max; i++) {
            SetLastError(0);
            if (!StackWalk64(machine_type, process, thread, &s, myCtx, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
                break;
            }

            if ( s.AddrReturn.Offset == 0 ) {
                break;
            }

            buffer[i] = (void*)s.AddrReturn.Offset;
            size++;
        }

        return size;
    }

    bool Backtrace::writeTo(ostream& out) const {
        const auto       process = GetCurrentProcess();
        SYMBOL_INFO*     symbol  = nullptr;
        IMAGEHLP_LINE64* line    = nullptr;
        bool             success = false;

        // Initialize symbol handler for symbol resolution
        initialize_symbols();

        symbol = (SYMBOL_INFO*)malloc(sizeof(SYMBOL_INFO) + 1023 * sizeof(TCHAR));
        if ( !symbol ) goto exit;
        symbol->MaxNameLen   = 1024;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        DWORD displacement;
        line = (IMAGEHLP_LINE64*)malloc(sizeof(IMAGEHLP_LINE64));
        if ( !line ) goto exit;
        line->SizeOfStruct = sizeof(IMAGEHLP_LINE64);

        for ( unsigned i = 0; i < _addrs.size(); i++ ) {
            if ( i > 0 ) out << "\r\n";
            out << '\t';
            const auto address = (DWORD64)_addrs[i];

            // Module basename
            void* moduleBase = nullptr;
            RtlPcToFileHeader(_addrs[i], &moduleBase);
            DWORD64 rva              = moduleBase ? (address - (DWORD64)moduleBase) : 0;
            char    modulePath[4096] = "?";
            if ( moduleBase ) {
                int mlen = GetModuleFileNameA((HMODULE)moduleBase, modulePath, sizeof(modulePath));
                if ( mlen > 0 ) {
                    char* p = modulePath + mlen - 1;
                    while ( p > modulePath && p[-1] != '\\' ) --p;
                    memmove(modulePath, p, strlen(p) + 1);
                }
            }

            // Function name and offset from symbol start
            DWORD64 fnOffset = 0;
            BOOL    symFound = SymFromAddr(process, address, &fnOffset, symbol);

            const char* fn   = symFound ? symbol->Name : "?";
            char*       cstr = nullptr;
            if ( SymGetLineFromAddr64(process, address, &displacement, line) ) {
                const char* fileBase = strrchr(line->FileName, '\\');
                fileBase             = fileBase ? fileBase + 1 : line->FileName;
                asprintf(&cstr, "%2u  %-25s %s + %llu  (%s:%lu)", i, modulePath, fn, fnOffset, fileBase,
                         line->LineNumber);
            } else {
                asprintf(&cstr, "%2u  %-25s %s + %llu  [0x%llx]", i, modulePath, fn, fnOffset, rva);
            }
            if ( !cstr ) goto exit;
            out << cstr;
            free(cstr);
        }
        success = true;

    exit:
        free(symbol);
        free(line);
        SymCleanup(process);
        return success;
    }

    static char* unmangle(const char* function) { return (char*)function; }

}  // namespace fleece

#endif  // _MSC_VER


#pragma mark - COMMON CODE:

namespace fleece {

    Backtrace::~Backtrace() { free(_symbols); }

    std::string Unmangle(const char* name NONNULL) {
        auto        unmangled = unmangle(name);
        std::string result    = unmangled;
        if ( unmangled != name ) free(unmangled);
        return result;
    }

    std::string Unmangle(const std::type_info& type) { return Unmangle(type.name()); }

    shared_ptr<Backtrace> Backtrace::capture(unsigned skipFrames, unsigned maxFrames, void* context) {
        // (By capturing afterwards, we avoid the (many) stack frames associated with make_shared)
        auto bt = make_shared<Backtrace>(0, 0);
        bt->_capture(skipFrames + 1, maxFrames, context);
        return bt;
    }

    Backtrace::Backtrace(unsigned skipFrames, unsigned maxFrames) {
        if ( maxFrames > 0 ) _capture(skipFrames + 1, maxFrames);
    }

    void Backtrace::_capture(unsigned skipFrames, unsigned maxFrames, void* context) {
        _addrs.resize(++skipFrames + maxFrames);  // skip this frame
        auto n = backtrace(&_addrs[0], skipFrames + maxFrames, context);
        _addrs.resize(n);
        skip(skipFrames);
    }

    void Backtrace::skip(unsigned nFrames) {
        _addrs.erase(_addrs.begin(), _addrs.begin() + min(size_t(nFrames), _addrs.size()));
    }

    string Backtrace::toString() const {
        stringstream out;
        writeTo(out);
        return out.str();
    }

    void Backtrace::writeCrashLog(ostream& out) {
        Backtrace bt(4);
        auto      xp = current_exception();
        if ( xp ) {
            out << "Uncaught exception:\n\t";
            try {
                rethrow_exception(xp);
            } catch ( const exception& x ) {
                const char* name      = typeid(x).name();
                char*       unmangled = unmangle(name);
                out << unmangled << ": " << x.what() << "\n";
                if ( unmangled != name ) free(unmangled);
            } catch ( ... ) { out << "unknown exception type\n"; }
        }
        out << "Backtrace:";
        bt.writeTo(out);
    }

    static BacktraceSignalHandler sSignalHandler = getSignalHandler();

    void Backtrace::handleTerminate(const function<void(const string&)>& logger) {
        // ---- Code below gets called by C++ runtime on an uncaught exception ---
        if ( logger ) {
            stringstream out;
            writeCrashLog(out);
            logger(out.str());
        } else {
            cerr << "\n\n******************** C++ fatal error ********************\n";
            writeCrashLog(cerr);
            cerr << "\n******************** Now terminating ********************\n";
        }
    }

    void Backtrace::installTerminateHandler(function<void(const string&)> logger) {
        static once_flag sOnce;
        call_once(sOnce, [&] {
            static auto const              sLogger     = std::move(logger);
            static terminate_handler const sOldHandler = set_terminate([] {
                // ---- Code below gets called by C++ runtime on a call to terminate ---
                handleTerminate(sLogger);
                // Chain to old handler:
                sOldHandler();
                // Just in case the old handler doesn't abort:
                abort();
                // ---- End of handler ----
            });

            static unexpected_handler const sOldUnexpectedHandler = set_unexpected([] {
                // ---- Code below gets called by C++ runtime on an unexpected exception (e.g. noexcept violation) ---
                handleTerminate(sLogger);
                // Chain to old handler:
                sOldUnexpectedHandler();
                // Just in case the old handler doesn't abort:
                abort();
                // ---- End of handler ----
            });

            BacktraceSignalHandler::setLogger(sLogger);
        });
    }

}  // namespace fleece
