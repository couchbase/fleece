#ifndef __APPLE__
#    error This file is only for Darwin (macOS/iOS) platforms
#endif

#include "Backtrace.hh"
#include <execinfo.h>
#include <dlfcn.h>
#include "betterassert.hh"

namespace fleece {
    Backtrace::frameInfo getFrame(const void* addr, bool stack_top) {
        Backtrace::frameInfo frame = {};
        frame.pc        = addr;

        Dl_info   info = {};
        if (!dladdr(addr, &info)) {
            return frame;
        }

        if (info.dli_fname) {
            frame.library = info.dli_fname;
            if ( const char* slash = strrchr(frame.library, '/') ) frame.library = slash + 1;
        }

        frame.function = info.dli_sname;
        if ( info.dli_saddr ) frame.offset = reinterpret_cast<size_t>(frame.pc) - reinterpret_cast<size_t>(info.dli_saddr);

        if ( info.dli_fbase ) {
            auto pc = reinterpret_cast<uintptr_t>(frame.pc);
            if ( !stack_top ) pc -= 1;
            frame.imageOffset = pc - reinterpret_cast<uintptr_t>(info.dli_fbase);
        }

        return frame;
    }

    int Backtrace::raw_capture(void** buffer, int max, void* context) {
        if ( max == 0 ) return 0;
        return backtrace(buffer, max);
    }

    const char* Backtrace::getSymbol(unsigned i) const {
        precondition(i < _addrs.size());
        if (!_symbols)
            _symbols = backtrace_symbols(_addrs.data(), int(_addrs.size()));

        if (_symbols) {
            const char* s = _symbols[i];

            // Skip line number and whitespace:
            while (*s && isdigit(*s))
                ++s;
            while (*s && isspace(*s))
                ++s;

            return s;
        }
        return nullptr;
    }
}  // namespace fleece
