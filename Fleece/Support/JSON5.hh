//
// JSON5.hh
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
#include <iostream>
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
