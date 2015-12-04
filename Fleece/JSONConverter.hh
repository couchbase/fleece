//
//  JSONConverter.hh
//  Fleece
//
//  Created by Jens Alfke on 11/21/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//

#ifndef Fleece_JSONConverter_h
#define Fleece_JSONConverter_h

#include "Encoder.hh"
#include "jsonsl.h"
#include <vector>
#include <map>

namespace fleece {

    /** Parses JSON data and writes the values in it to a Fleece encoder. */
    class JSONConverter {
    public:
        JSONConverter(Encoder&);
        ~JSONConverter();

        /** Parses JSON data and writes the values to the encoder.
            @return  True if parsing succeeded, false if the JSON is invalid. */
        bool convertJSON(slice json);

        /** See jsonsl_error_t for error codes, plus a few more defined below. */
        int error()                 {return _error;}
        
        /** Byte offset in input where error occurred */
        size_t errorPos()           {return _errorPos;}

        /** Extra error codes beyond those in jsonsl_error_t. */
        enum {
            kErrTruncatedJSON = 1000
        };

    private:
        bool countJSONItems(slice json);
        void push(struct jsonsl_state_st *state);
        void pop(struct jsonsl_state_st *state);
        static int errorCallback(jsonsl_t jsn,
                                 jsonsl_error_t err,
                                 struct jsonsl_state_st *state,
                                 char *errat);
        static void writePushCallback(jsonsl_t jsn,
                                      jsonsl_action_t action,
                                      struct jsonsl_state_st *state,
                                      const char *buf);
        static void writePopCallback(jsonsl_t jsn,
                                     jsonsl_action_t action,
                                     struct jsonsl_state_st *state,
                                     const char *buf);

        typedef std::map<size_t, uint64_t> startToLengthMap;

        Encoder &_encoder;                  // encoder to write to
        jsonsl_t _jsn;                      // JSON parser
        int _error;                         // Parse error from jsonsl
        size_t _errorPos;                   // Byte index where parse error occurred
        slice _input;                       // Current JSON being parsed
    };

}

#endif /* Fleece_JSONConverter_h */
