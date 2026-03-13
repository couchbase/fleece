#ifndef _WIN32
#    error "This implementation is only for Windows"
#endif

#include "Backtrace.hh"
#include <Windows.h>

namespace fleece {
    char* unmangle(const char* function) { return const_cast<char*>(function); }

    int Backtrace::raw_capture(void** buffer, int max, void* context) {
        CONTEXT  localCtx;
        CONTEXT* myCtx;
        if ( context ) {
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
        HANDLE       thread  = GetCurrentThread();
        auto         process = GetCurrentProcess();
        STACKFRAME64 s{};

        s.AddrStack.Mode = AddrModeFlat;
        s.AddrFrame.Mode = AddrModeFlat;
        s.AddrPC.Mode    = AddrModeFlat;
#if defined(_M_X64)
        s.AddrPC.Offset    = myCtx->Rip;
        s.AddrStack.Offset = myCtx->Rsp;
        s.AddrFrame.Offset = myCtx->Rbp;
        auto machine_type  = IMAGE_FILE_MACHINE_AMD64;
#elif defined(_M_ARM64)
        s.AddrPC.Offset    = myCtx->Pc;
        s.AddrStack.Offset = myCtx->Sp;
        s.AddrFrame.Offset = myCtx->R11;
        auto machine_type  = IMAGE_FILE_MACHINE_ARM64;
#else
#    error Unsupported architecture
#endif

        auto size = 0;
        for ( int i = 0; i < max; i++ ) {
            SetLastError(0);
            if ( !StackWalk64(machine_type, process, thread, &s, myCtx, NULL, SymFunctionTableAccess64,
                              SymGetModuleBase64, NULL) ) {
                break;
            }

            if ( s.AddrReturn.Offset == 0 ) { break; }

            buffer[i] = reinterpret_cast<void*>(s.AddrReturn.Offset);
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
        return success;
    }
}  // namespace fleece
