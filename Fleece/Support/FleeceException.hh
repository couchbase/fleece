//
// FleeceException.hh
//
// Copyright 2016-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include <stdexcept>
#include "fleece/PlatformCompat.hh"
#include <memory>

namespace fleece {
    class Backtrace;

    // Error codes -- keep these in sync with the public FLError enum in Fleece.h!
    typedef enum {
        NoError = 0,
        MemoryError,        // Out of memory, or allocation failed
        OutOfRange,         // Array index or iterator out of range
        InvalidData,        // Bad input data (NaN, non-string key, etc.)
        EncodeError,        // Structural error encoding (missing value, too many ends, etc.)
        JSONError,          // Error parsing JSON
        UnknownValue,       // Unparseable data in a Value (corrupt? Or from some distant future?)
        PathSyntaxError,    // Invalid Path specifier
        InternalError,      // This shouldn't happen
        NotFound,           // Key not found
        SharedKeysStateError, // Incorrect use of persistent shared keys (not in transaction, etc.)
        POSIXError,         // Error from C/POSIX call; see .errno for details
    } ErrorCode;


    class FleeceException : public std::runtime_error {
    public:

        FleeceException(ErrorCode code_, int errno_, const std::string &what);

        [[noreturn]] static void _throw(ErrorCode code, const char *what, ...) __printflike(2,3);
        [[noreturn]] static void _throwErrno(const char *what, ...) __printflike(1,2);

        static ErrorCode getCode(const std::exception&) noexcept;

        const ErrorCode code;
        const int err_no {0};
        std::shared_ptr<Backtrace> backtrace;
    };

    #define throwIf(BAD, ERROR, MESSAGE, ...) \
     if (_usuallyTrue(!(BAD))) ; else fleece::FleeceException::_throw(ERROR, MESSAGE, ##__VA_ARGS__)

}
