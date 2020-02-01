//
// FleeceException.cc
//
// Copyright (c) 2016 Couchbase, Inc All rights reserved.
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

#include "FleeceException.hh"
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
