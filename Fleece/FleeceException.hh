//
// FleeceException.hh
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

#pragma once
#include <stdexcept>
#include "PlatformCompat.hh"

namespace fleece {

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
    } ErrorCode;


    class FleeceException : public std::runtime_error {
    public:

        FleeceException(ErrorCode code_, const std::string &what)
        :std::runtime_error(what)
        ,code(code_)
        { }

        [[noreturn]] static void _throw(ErrorCode code, const char *what);

        static ErrorCode getCode(const std::exception&) noexcept;

        const ErrorCode code;
    };


    static inline void throwIf(bool bad, ErrorCode error, const char *message) {
        if (_usuallyFalse(bad))
            FleeceException::_throw(error, message);
    }

}
