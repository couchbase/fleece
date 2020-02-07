//
// Backtrace.cc
//
// Copyright © 2018 Couchbase. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "Backtrace.hh"
#include <csignal>
#include <exception>
#include <mutex>
#include <sstream>
#include <string.h>


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

#ifdef __clang__
    #include <cxxabi.h>     // abi::__cxa_demangle()
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


    Backtrace::Backtrace(unsigned skip)
    :_skip(skip + 1)    // skip the constructor frame itself
    ,_nAddrs(backtrace(_addrs, kMaxAddrs))
    { }


#ifdef __clang__
    Backtrace::~Backtrace() {
        free(_unmangled);
    }
#endif


    Backtrace::frameInfo Backtrace::getFrame(unsigned i) {
        frameInfo frame = { };
        // On Android, use dladdr() to get info about the PC address:
        Dl_info info;
        if (dladdr(_addrs[i], &info)) {
            frame.pc = (size_t)_addrs[i];
            frame.function = info.dli_sname;
            frame.offset = frame.pc - (size_t)info.dli_saddr;
            frame.library = info.dli_fname;
            const char *slash = strrchr(frame.library, '/');
            if (slash)
                frame.library = slash + 1;
        }
        return frame;
    }


    const char* Backtrace::unmangle(const char *function) {
#ifdef __clang__
        int status;
        _unmangled = abi::__cxa_demangle(function, _unmangled, &_unmangledLen, &status);
        if (_unmangled && status == 0)
            function = _unmangled;
#endif
        return function;
    }


    bool Backtrace::writeTo(ostream &out) {
        if (_skip >= _nAddrs)
            return false;

        for (int i = _skip; i < _nAddrs; ++i) {
            out << "\n\t";
            char *cstr = nullptr;
            auto frame = getFrame(i);
            int len;
            if (frame.function) {
                len = asprintf(&cstr, "%2d  %-25s %s + %zd",
                               i, frame.library, unmangle(frame.function), frame.offset);
            } else {
                len = asprintf(&cstr, "%2d  %p", i, _addrs[i]);
            }
            if (len < 0)
                return false;
            out.write(cstr, size_t(len));
            free(cstr);
        }
        return true;
    }

}


#else // _MSC_VER
#pragma mark - WINDOWS IMPLEMENTATION:

#pragma comment(lib, "Dbghelp.lib")
#include <Windows.h>
#include <Dbghelp.h>
#include "asprintf.h"
#include <sstream>
using namespace std;

namespace fleece {

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

    Backtrace::Backtrace(unsigned skip)
    :_skip(skip + 1)    // skip the constructor frame itself
    {
        _nAddrs = CaptureStackBackTrace(0, 50, _addrs, nullptr);
    }


    bool Backtrace::writeTo(ostream &out) {
        const auto process = GetCurrentProcess();
        SYMBOL_INFO *symbol = nullptr;
        IMAGEHLP_LINE64 *line = nullptr;
        bool success = false;
        SymInitialize(process, nullptr, TRUE);

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

        for (unsigned i = _skip + 1; i < _nAddrs; i++) {
            out << "\r\n\t";
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
    // Windows Store apps cannot get backtraces
    Backtrace::Backtrace(unsigned skipFrames)   :_skip(skipFrames) { }
    bool Backtrace::writeTo(std::ostream&)      {return false;}
#endif

    const char* Backtrace::unmangle(const char *symbol) {
        return symbol;
    }

}

#endif // _MSC_VER


namespace fleece {

    string Backtrace::toString() {
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
                out << bt.unmangle(name) << ": " <<  x.what() << "\n";
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
            static const auto sLogger = logger;
            static const terminate_handler sOldHandler = set_terminate([] {
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
