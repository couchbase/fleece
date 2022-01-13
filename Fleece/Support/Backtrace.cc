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


#ifndef _MSC_VER
#pragma mark - UNIX IMPLEMENTATION:

#include <dlfcn.h>          // dladdr()

#ifndef __ANDROID__
    #define HAVE_EXECINFO
    #include <execinfo.h>   // backtrace(), backtrace_symbols()
#else
    #include <stdlib.h>     // free
    #include <unwind.h>     // _Unwind_Backtrace(), etc.
#endif

#if defined(_LIBCPP_VERSION) || defined(__ANDROID__)
    #include <cxxabi.h>     // abi::__cxa_demangle()
    #define HAVE_UNMANGLE
#endif


namespace fleece {
    using namespace std;


#ifndef HAVE_EXECINFO
    // Use libunwind to emulate backtrace(). This is limited in that it won't traverse any stack
    // frames before a signal handler, so it's not useful for logging the stack upon a crash.
    // Adapted from https://stackoverflow.com/a/28858941
    // See also https://stackoverflow.com/q/29559347 for more details & possible workarounds

    struct BacktraceState {
        void** current;
        void** end;
    };

    static _Unwind_Reason_Code unwindCallback(struct _Unwind_Context* context, void* arg) {
        BacktraceState* state = static_cast<BacktraceState*>(arg);
        uintptr_t pc = _Unwind_GetIP(context);
        if (pc) {
            if (state->current == state->end) {
                return _URC_END_OF_STACK;
            } else {
                *state->current++ = reinterpret_cast<void*>(pc);
            }
        }
        return _URC_NO_REASON;
    }

    static int backtrace(void** buffer, size_t max) {
        BacktraceState state = {buffer, buffer + max};
        _Unwind_Backtrace(unwindCallback, &state);

        return int(state.current - buffer);
    }
#endif


    static char* unmangle(const char *function) {
#ifdef HAVE_UNMANGLE
        int status;
        size_t unmangledLen;
        char *unmangled = abi::__cxa_demangle(function, nullptr, &unmangledLen, &status);
        if (unmangled && status == 0)
            return unmangled;
        free(unmangled);
#endif
        return (char*)function;
    }


    Backtrace::frameInfo Backtrace::getFrame(unsigned i) const {
        precondition(i < _addrs.size());
        frameInfo frame = { };
        Dl_info info;
        if (dladdr(_addrs[i], &info)) {
            frame.pc = _addrs[i];
            frame.offset = (size_t)frame.pc - (size_t)info.dli_saddr;
            frame.function = info.dli_sname;
            frame.library = info.dli_fname;
            const char *slash = strrchr(frame.library, '/');
            if (slash)
                frame.library = slash + 1;
        }
        return frame;
    }


    // If any of these strings occur in a backtrace, suppress further frames.
    static constexpr const char* kTerminalFunctions[] = {
        "_C_A_T_C_H____T_E_S_T_",
        "Catch::TestInvokerAsFunction::invoke() const",
        "litecore::actor::Scheduler::task(unsigned)",
        "litecore::actor::GCDMailbox::safelyCall",
    };

    static constexpr struct {const char *old, *nuu;} kAbbreviations[] = {
        {"(anonymous namespace)",   "(anon)"},
        {"std::__1::",              "std::"},
        {"std::basic_string<char, std::char_traits<char>, std::allocator<char> >",
                                    "std::string"},
    };


    static void replace(std::string &str, string_view oldStr, string_view newStr) {
        string::size_type pos = 0;
        while (string::npos != (pos = str.find(oldStr, pos))) {
            str.replace(pos, oldStr.size(), newStr);
            pos += newStr.size();
        }
    }


    bool Backtrace::writeTo(ostream &out) const {
        for (int i = 0; i < _addrs.size(); ++i) {
            if (i > 0)
                out << '\n';
            out << '\t';
            char *cstr = nullptr;
            auto frame = getFrame(i);
            int len;
            bool stop = false;
            if (frame.function) {
                string name = Unmangle(frame.function);
                // Stop when we hit a unit test, or other known functions:
                for (auto fn : kTerminalFunctions) {
                    if (name.find(fn) != string::npos)
                        stop = true;
                }
                // Abbreviate some C++ verbosity:
                for (auto &abbrev : kAbbreviations)
                    replace(name, abbrev.old, abbrev.nuu);
                len = asprintf(&cstr, "%2d  %-25s %s + %zd",
                               i, frame.library, name.c_str(), frame.offset);
            } else {
                len = asprintf(&cstr, "%2d  %p", i, _addrs[i]);
            }
            if (len < 0)
                return false;
            out.write(cstr, size_t(len));
            free(cstr);

            if (stop) {
                out << "\n\t ... (" << (_addrs.size() - i - 1) << " more suppressed) ...";
                break;
            }
        }
        return true;
    }


    std::string FunctionName(const void *pc) {
        Dl_info info = {};
        dladdr(pc, &info);
        if (info.dli_sname)
            return Unmangle(info.dli_sname);
        else
            return "";
    }

}


#else // _MSC_VER
#pragma mark - WINDOWS IMPLEMENTATION:

#pragma comment(lib, "Dbghelp.lib")
#include <Windows.h>
#include <Dbghelp.h>
#include "asprintf.h"
#include <sstream>
#include <iomanip>
using namespace std;

namespace fleece {

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

    static inline int backtrace(void** buffer, size_t max) {
        return (int)CaptureStackBackTrace(0, (DWORD)max, buffer, nullptr);
    }


    bool Backtrace::writeTo(ostream &out) const {
        const auto process = GetCurrentProcess();
        SYMBOL_INFO *symbol = nullptr;
        IMAGEHLP_LINE64 *line = nullptr;
        bool success = false;
        SymInitialize(process, nullptr, TRUE);
        DWORD symOptions = SymGetOptions();
        symOptions |= SYMOPT_LOAD_LINES | SYMOPT_UNDNAME;
        SymSetOptions(symOptions);

        symbol = (SYMBOL_INFO*)malloc(sizeof(SYMBOL_INFO)+1023 * sizeof(TCHAR));
        if (!symbol)
            goto exit;
        symbol->MaxNameLen = 1024;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        DWORD displacement;
        line = (IMAGEHLP_LINE64*)malloc(sizeof(IMAGEHLP_LINE64));
        if (!line)
            goto exit;
        line->SizeOfStruct = sizeof(IMAGEHLP_LINE64);

        for (unsigned i = 0; i < _addrs.size(); i++) {
            if (i > 0)
                out << "\r\n";
            out << '\t';
            const auto address = (DWORD64)_addrs[i];
            SymFromAddr(process, address, nullptr, symbol);
            char* cstr = nullptr;
            if (SymGetLineFromAddr64(process, address, &displacement, line)) {
                asprintf(&cstr, "at %s in %s: line: %lu: address: 0x%0llX",
                         symbol->Name, line->FileName, line->LineNumber, symbol->Address);
            } else {
                asprintf(&cstr, "at %s, address 0x%0llX",
                         symbol->Name, symbol->Address);
            }
            if (!cstr)
                goto exit;
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

#else
    static inline int backtrace(void** buffer, size_t max) {
        return (int)CaptureStackBackTrace(0, (DWORD)max, buffer, nullptr);
    }

    // Symbolication is not possible within a UWP application, has to be performed
    // using external tools.
    bool Backtrace::writeTo(ostream& out) const  {
        for (unsigned i = 0; i < _addrs.size(); i++) {
            void* moduleBase = nullptr;
            RtlPcToFileHeader(_addrs[i], &moduleBase);
            auto moduleBaseTyped = (const unsigned char *)moduleBase;
            char modulePath[4096];
            if(moduleBase) {
                int length = GetModuleFileNameA((HMODULE)moduleBase, modulePath, 4096) - 1;
                char* pos = modulePath + length;
                if(length > 0) {
                    while(length-- > 0 && pos[0] != '\\') {
                        pos--;
                    }
                } else {
                    strcpy(pos, "<unknown module name> ");
                }

                if(i > 0) {
                    out << "\r\n"; 
                }

                out << '\t';
                out << pos << "+0x" << setw(sizeof(size_t) * 2) << setfill('0') << hex << (uint32_t)((unsigned char *)_addrs[i] - moduleBaseTyped);
            } else {
                out << "\t<unknown module> 0x" << setw(sizeof(size_t) * 2) << setfill('0') << hex << _addrs[i];
            }
        }

        return true;
    }
#endif


    static char* unmangle(const char *function) {
        return (char*)function;
    }

}

#endif // _MSC_VER


#pragma mark - COMMON CODE:


namespace fleece {


    std::string Unmangle(const char *name NONNULL) {
        auto unmangled = unmangle(name);
        std::string result = unmangled;
        if (unmangled != name)
            free(unmangled);
        return result;
    }


    std::string Unmangle(const std::type_info &type) {
        return Unmangle(type.name());
    }


    shared_ptr<Backtrace> Backtrace::capture(unsigned skipFrames, unsigned maxFrames) {
        // (By capturing afterwards, we avoid the (many) stack frames associated with make_shared)
        auto bt = make_shared<Backtrace>(0, 0);
        bt->_capture(skipFrames + 1, maxFrames);
        return bt;
    }

    Backtrace::Backtrace(unsigned skipFrames, unsigned maxFrames) {
        if (maxFrames > 0)
            capture(skipFrames + 1, maxFrames);
    }


    void Backtrace::_capture(unsigned skipFrames, unsigned maxFrames) {
        _addrs.resize(++skipFrames + maxFrames);        // skip this frame
        auto n = backtrace(&_addrs[0], skipFrames + maxFrames);
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


    void Backtrace::writeCrashLog(ostream &out) {
        Backtrace bt(4);
        auto xp = current_exception();
        if (xp) {
            out << "Uncaught exception:\n\t";
            try {
                rethrow_exception(xp);
            } catch(const exception& x) {
                const char *name = typeid(x).name();
                char *unmangled = unmangle(name);
                out << unmangled << ": " <<  x.what() << "\n";
                if (unmangled != name)
                    free(unmangled);
            } catch (...) {
                out << "unknown exception type\n";
            }
        }
        out << "Backtrace:";
        bt.writeTo(out);
    }


    void Backtrace::installTerminateHandler(function<void(const string&)> logger) {
        static once_flag sOnce;
        call_once(sOnce, [&] {
            static auto const sLogger = move(logger);
            static terminate_handler const sOldHandler = set_terminate([] {
                // ---- Code below gets called by C++ runtime on an uncaught exception ---
                if (sLogger) {
                    stringstream out;
                    writeCrashLog(out);
                    sLogger(out.str());
                } else {
                    cerr << "\n\n******************** C++ fatal error ********************\n";
                    writeCrashLog(cerr);
                    cerr << "\n******************** Now terminating ********************\n";
                }
                // Chain to old handler:
                sOldHandler();
                // Just in case the old handler doesn't abort:
                abort();
                // ---- End of handler ----
            });
        });
    }

}
