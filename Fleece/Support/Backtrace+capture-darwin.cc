#ifndef __APPLE__
#    error This file is only for Darwin (macOS/iOS) platforms
#endif

#include "Backtrace.hh"
#include <execinfo.h>

namespace fleece {
    int Backtrace::raw_capture(void** buffer, int max, void* context) {
        if ( max == 0 ) return 0;
        return backtrace(buffer, max);
    }
}  // namespace fleece
