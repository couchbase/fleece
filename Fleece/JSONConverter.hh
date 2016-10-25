//
//  JSONConverter.hh
//  Fleece
//
//  Created by Jens Alfke on 11/21/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//

#pragma once

#include "Encoder.hh"
#include "slice.hh"
#include <vector>
#include <map>

extern "C" {
    struct jsonsl_state_st;
    struct jsonsl_st;
}

namespace fleece {

    /** Parses JSON data and writes the values in it to a Fleece encoder. */
    class JSONConverter {
    public:
        JSONConverter(Encoder&) noexcept;
        ~JSONConverter();

        /** Parses JSON data and writes the values to the encoder.
            @return  True if parsing succeeded, false if the JSON is invalid. */
        bool encodeJSON(slice json);

        /** See jsonsl_error_t for error codes, plus a few more defined below. */
        int error() noexcept                    {return _error;}
        const char* errorMessage() noexcept;
        
        /** Byte offset in input where error occurred */
        size_t errorPos() noexcept              {return _errorPos;}

        /** Extra error codes beyond those in jsonsl_error_t. */
        enum {
            kErrTruncatedJSON = 1000
        };

        /** Resets the converter, as though you'd deleted it and constructed a new one. */
        void reset();

        /** Convenience method to convert JSON to Fleece data. Throws FleeceException on error. */
        static alloc_slice convertJSON(slice json, SharedKeys *sk =nullptr);

    //private:
        void push(struct jsonsl_state_st *state);
        void pop(struct jsonsl_state_st *state);
        int gotError(int err, size_t pos) noexcept;
        int gotError(int err, const char *errat) noexcept;

    private:
        typedef std::map<size_t, uint64_t> startToLengthMap;

        Encoder &_encoder;                  // encoder to write to
        struct jsonsl_st * _jsn;            // JSON parser
        int _error;                         // Parse error from jsonsl
        size_t _errorPos;                   // Byte index where parse error occurred
        slice _input;                       // Current JSON being parsed
    };

}
