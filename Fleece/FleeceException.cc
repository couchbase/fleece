//
//  FleeceException.cc
//  Fleece
//
//  Created by Jens Alfke on 7/19/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//

#include "FleeceException.hh"
#include <memory>
#include <string>


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
        "incorrect use of persistent hsared keys",
    };

    void FleeceException::_throw(ErrorCode code, const char *what) {
        std::string message = kErrorNames[code];
        if (what)
            message += std::string(": ") + what;
        throw FleeceException(code, message);
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
