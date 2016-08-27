//
//  Encoder+JSON.cc
//  Fleece
//
//  Created by Jens Alfke on 11/21/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "JSONConverter.hh"
#include "jsonsl.h"
#include <map>

namespace fleece {

    static int errorCallback(struct jsonsl_st * jsn,
                             jsonsl_error_t err,
                             struct jsonsl_state_st *state,
                             char *errat);
    static void writePushCallback(struct jsonsl_st * jsn,
                                  jsonsl_action_t action,
                                  struct jsonsl_state_st *state,
                                  const char *buf);
    static void writePopCallback(struct jsonsl_st * jsn,
                                 jsonsl_action_t action,
                                 struct jsonsl_state_st *state,
                                 const char *buf);

    JSONConverter::JSONConverter(Encoder &e)
    :_encoder(e),
     _jsn(jsonsl_new(0x2000)),
     _error(JSONSL_ERROR_SUCCESS),
     _errorPos(0)
    {
        if (!_jsn)
            throw std::bad_alloc();
        _jsn->data = this;
    }

    JSONConverter::~JSONConverter() {
        if (_jsn)
            jsonsl_destroy(_jsn);
    }

    bool JSONConverter::convertJSON(slice json) {
        _input = json;
        _error = JSONSL_ERROR_SUCCESS;
        _errorPos = 0;

        _jsn->data = this;
        _jsn->action_callback_PUSH = writePushCallback;
        _jsn->action_callback_POP  = writePopCallback;
        _jsn->error_callback = errorCallback;
        jsonsl_enable_all_callbacks(_jsn);

        jsonsl_feed(_jsn, (char*)json.buf, json.size);
        if (_jsn->level > 0 && !_error) {
            // Input is valid JSON so far, but truncated:
            _error = kErrTruncatedJSON;
            _errorPos = json.size;
        }
        jsonsl_reset(_jsn);
        return (_error == JSONSL_ERROR_SUCCESS);
    }

    inline void JSONConverter::push(struct jsonsl_state_st *state) {
        switch (state->type) {
            case JSONSL_T_LIST:
                _encoder.beginArray();
                break;
            case JSONSL_T_OBJECT:
                _encoder.beginDictionary();
                break;
        }
    }

    inline void JSONConverter::pop(struct jsonsl_state_st *state) {
        switch (state->type) {
            case JSONSL_T_SPECIAL: {
                unsigned f = state->special_flags;
                if (f & JSONSL_SPECIALf_FLOAT) {
                    char *start = (char*)&_input[state->pos_begin];
                    char *end;
                    double n = ::strtod(start, &end);
                    _encoder.writeDouble(n);
                } else if (f & JSONSL_SPECIALf_UNSIGNED) {
                    _encoder.writeUInt(state->nelem);
                } else if (f & JSONSL_SPECIALf_SIGNED) {
                    _encoder.writeInt(-(int64_t)state->nelem);
                } else if (f & JSONSL_SPECIALf_TRUE) {
                    _encoder.writeBool(true);
                } else if (f & JSONSL_SPECIALf_FALSE) {
                    _encoder.writeBool(false);
                } else if (f & JSONSL_SPECIALf_NULL) {
                    _encoder.writeNull();
                }
                break;
            }
            case JSONSL_T_STRING:
            case JSONSL_T_HKEY: {
                slice str(&_input[state->pos_begin + 1],
                          state->pos_cur - state->pos_begin - 1);
                char *buf = NULL;
                bool mallocedBuf = false;
                if (state->nescapes > 0) {
                    // De-escape str:
                    mallocedBuf = str.size > 100;
                    buf = (char*)(mallocedBuf ? malloc(str.size) : alloca(str.size));
                    jsonsl_error_t err = JSONSL_ERROR_SUCCESS;
                    const char *errat;
                    auto size = jsonsl_util_unescape_ex((const char*)str.buf, buf, str.size,
                                                        NULL, NULL, &err, &errat);
                    if (err) {
                        errorCallback(_jsn, err, state, (char*)errat);
                        if (mallocedBuf)
                            free(buf);
                        return;
                    }
                    str = slice(buf, size);
                }
                if (state->type == JSONSL_T_STRING)
                    _encoder.writeString(str);
                else
                    _encoder.writeKey(str);
                if (mallocedBuf)
                    free(buf);
                break;
            }
            case JSONSL_T_LIST:
                _encoder.endArray();
                break;
            case JSONSL_T_OBJECT:
                _encoder.endDictionary();
                break;
        }
    }

    inline int JSONConverter::gotError(int err, char *errat) {
        _error = err;
        _errorPos = errat - (char*)_input.buf;
        jsonsl_stop(_jsn);
        return 0;
    }


    // Callbacks:

    static inline JSONConverter* converter(jsonsl_t jsn) {
        return (JSONConverter*)jsn->data;
    }
    
    static void writePushCallback(jsonsl_t jsn,
                                  jsonsl_action_t action,
                                  struct jsonsl_state_st *state,
                                  const char *buf)
    {
        converter(jsn)->push(state);
    }

    static void writePopCallback(jsonsl_t jsn,
                                 jsonsl_action_t action,
                                 struct jsonsl_state_st *state,
                                 const char *buf)
    {
        converter(jsn)->pop(state);
    }

    static int errorCallback(jsonsl_t jsn,
                             jsonsl_error_t err,
                             struct jsonsl_state_st *state,
                             char *errat)
    {
        return converter(jsn)->gotError(err, errat);
    }

}
