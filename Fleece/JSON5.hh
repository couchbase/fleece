//
//  JSON5.hh
//  Fleece
//
//  Created by Jens Alfke on 12/13/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include <iostream>

namespace fleece {

    // Reads valid JSON5 from a stream and writes the equivalent JSON to another stream.
    // Given _invalid_ JSON5, it either throws a runtime_exception or produces invalid JSON.
    void ConvertJSON5(std::istream &in, std::ostream &out);

    // Converts a valid JSON5 string to an equivalent JSON string.
    std::string ConvertJSON5(const std::string &in);

    // For more info visit http://json5.org

}
