//
// FleeceException.cc
//
// Copyright 2016-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "FleeceException.hh"
#include "fleece/PlatformCompat.hh"
#include "Backtrace.hh"
#include <errno.h>
#include <memory>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>

#ifdef _MSC_VER
#include "asprintf.h"
#endif

namespace fleece {

    static const char* const kErrorNames[] = {
        "",
        "memory error",
        "array/iterator index out of range",
        "invalid input data",
        "encoder error",
        "JSON error",
        "unknown Fleece value; data may be corrupt",
        "Fleece path syntax error",
        "internal Fleece library error",
        "key not found",
        "incorrect use of persistent shared keys",
        "POSIX error",
        "unsupported operation",
    };


    FleeceException::FleeceException(ErrorCode code_, int errno_, const std::string &what)
    :std::runtime_error(what)
    ,code(code_)
    ,err_no(errno_)
    {
        if (code_ != OutOfRange) {
            backtrace = Backtrace::capture(2);
        }
    }


    __cold
    void FleeceException::_throw(ErrorCode code, const char *what, ...) {
        std::string message = kErrorNames[code];
        if (what) {
            va_list args;
            va_start(args, what);
            char *msg;
            int len = vasprintf(&msg, what, args);
            va_end(args);
            if (len >= 0) {
                message += std::string(": ") + msg;
                free(msg);
            }
        }
        throw FleeceException(code, 0, message);
    }


    __cold
    void FleeceException::_throwErrno(const char *what, ...) {
        va_list args;
        va_start(args, what);
        char *msg;
        int len = vasprintf(&msg, what, args);
        va_end(args);
        std::string message;
        if (len >= 0) {
            message = std::string(msg) + ": " + strerror(errno);
            free(msg);
        }
        throw FleeceException(POSIXError, errno, message);
    }


    __cold
    ErrorCode FleeceException::getCode(const std::exception &x) noexcept {
        auto fleecex = dynamic_cast<const FleeceException*>(&x);
        if (fleecex)
            return fleecex->code;
        else if (nullptr != dynamic_cast<const std::bad_alloc*>(&x))
            return MemoryError;
        else
            return InternalError;
    }

}
