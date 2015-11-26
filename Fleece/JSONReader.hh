//
//  JSONReader.hh
//  Fleece
//
//  Created by Jens Alfke on 11/21/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//

#ifndef Fleece_JSONReader_h
#define Fleece_JSONReader_h

#include "Encoder.hh"
#include "jsonsl.h"
#include <vector>
#include <map>

namespace fleece {

    class JSONReader {
    public:
        JSONReader(encoder&);
        ~JSONReader();

        /** Parses JSON data and writes the value contained in it to the Fleece encoder. */
        bool writeJSON(slice json);

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

        encoder &_encoder;                  // encoder to write to
        jsonsl_t _jsn;                      // JSON parser
        int _error;                         // Parse error from jsonsl
        size_t _errorPos;                   // Byte index where parse error occurred
        slice _input;                       // Current JSON being parsed
    };

}

#endif /* Fleece_JSONReader_h */
