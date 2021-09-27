//
// JSON5.hh
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
#include <iosfwd>
#include <stdexcept>
#include <string>

namespace fleece {

    /// Reads valid JSON5 from a stream and writes the equivalent JSON to another stream.
    /// Given _invalid_ JSON5, it either throws a \ref json5_error or produces invalid JSON.
    /// (It detects structural errors, but does not detect invalid UTF-8 or check the innards
    /// of strings or numbers.)
    /// For more info visit https://json5.org
    void ConvertJSON5(std::istream &in, std::ostream &out);

    /// Converts a valid JSON5 string to an equivalent JSON string.
    /// Given _invalid_ JSON5, it either throws a \ref json5_error or returns invalid JSON.
    /// (It detects structural errors, but does not detect invalid UTF-8 or check the innards
    /// of strings or numbers.)
    /// For more info visit https://json5.org
    std::string ConvertJSON5(const std::string &in);

    /// Parse error thrown by \ref ConvertJSON5. Includes the approximate position in the input.
    class json5_error : public std::runtime_error {
    public:
        json5_error(const std::string &what, std::string::size_type inputPos_);
        std::string::size_type const inputPos;
    };


}
