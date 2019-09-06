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
#include "PlatformCompat.hh"
#include <errno.h>
#include <memory>
#include <stdarg.h>
#include <stdlib.h>
#include <string>

#ifdef _MSC_VER
#include "asprintf.h"
#include <windows.h>
std::string cbl_strerror(int err) {
    if(err < sys_nerr) {
        // As of Windows 10, only errors 0 - 42 have a message in strerror
        return strerror(err);
    }

    // Hope the POSIX definitions don't change...
    if(err < 100 || err > 140) {
        return "Unknown Error";
    }

    static long wsaEquivalent[] = {
        WSAEADDRINUSE,
        WSAEADDRNOTAVAIL,
        WSAEAFNOSUPPORT,
        WSAEALREADY,
        0,
        WSAECANCELLED,
        WSAECONNABORTED,
        WSAECONNREFUSED,
        WSAECONNRESET,
        WSAEDESTADDRREQ,
        WSAEHOSTUNREACH,
        0,
        WSAEINPROGRESS,
        WSAEISCONN,
        WSAELOOP,
        WSAEMSGSIZE,
        WSAENETDOWN,
        WSAENETRESET,
        WSAENETUNREACH,
        WSAENOBUFS,
        0,
        0,
        0,
        WSAENOPROTOOPT,
        0,
        0,
        WSAENOTCONN,
        0,
        WSAENOTSOCK,
        0,
        WSAEOPNOTSUPP,
        0,
        0,
        0,
        0,
        WSAEPROTONOSUPPORT,
        WSAEPROTOTYPE,
        0,
        WSAETIMEDOUT,
        0,
        WSAEWOULDBLOCK
    };

    const long equivalent = wsaEquivalent[err - 100];
    if(equivalent == 0) {
        return "Unknown Error";
    }

    char buf[1024];
    buf[0] = 0;
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, equivalent, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buf, sizeof(buf), nullptr);
    return std::string(buf);
}
#endif

namespace fleece {

    static const char* kErrorNames[] = {
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

    void FleeceException::_throw(ErrorCode code, const char *what, ...) {
        std::string message = kErrorNames[code];
        if (what) {
            va_list args;
            va_start(args, what);
            char *msg;
            vasprintf(&msg, what, args);
            va_end(args);
            message += std::string(": ") + msg;
            free(msg);
        }
        throw FleeceException(code, 0, message);
    }


    void FleeceException::_throwErrno(const char *what, ...) {
        va_list args;
        va_start(args, what);
        char *msg;
        vasprintf(&msg, what, args);
        va_end(args);
        auto message = std::string(msg) + ": " + cbl_strerror(errno);
        free(msg);
        throw FleeceException(POSIXError, errno, message);
    }


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
