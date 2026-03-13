#ifndef __APPLE__
#    error This file is only for Darwin (macOS/iOS) platforms
#endif

#include "Backtrace.hh"
#include <execinfo.h>

namespace fleece {
    Backtrace::frameInfo getFrame(const void* addr, bool stack_top) {
        frameInfo frame = {};
        Dl_info   info;
        if ( dladdr(addr, &info) ) {
            frame.pc          = stack_top ? addr : addr - 1;
            frame.offset      = (size_t)frame.pc - (size_t)info.dli_saddr;
            frame.function    = info.dli_sname;
            frame.library     = info.dli_fname;
            const char* slash = strrchr(frame.library, '/');
            if ( slash ) frame.library = slash + 1;
        }

        return frame;
    }

    int Backtrace::raw_capture(void** buffer, int max, void* context) {
        if ( max == 0 ) return 0;
        return backtrace(buffer, max);
    }
}  // namespace fleece
