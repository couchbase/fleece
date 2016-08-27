//
//  FleeceException.hh
//  Fleece
//
//  Created by Jens Alfke on 7/19/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//

#pragma once
#include <exception>

namespace fleece {

    class FleeceException : public std::exception {
    public:
        FleeceException(const char *what)
        :_what(what)
        { }

        virtual const char* what() const noexcept override {
            return _what;
        }

    private:
        const char* _what;
    };
    
}
