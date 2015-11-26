//
//  Encoder+JSON.cc
//  Fleece
//
//  Created by Jens Alfke on 11/21/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//

#include "JSONReader.hh"
#include "jsonsl.h"
#include <map>

namespace fleece {

    JSONReader::JSONReader(encoder &e)
    :_jsn(jsonsl_new(0x2000)),
     _encoder(e),
     _error(0),
     _errorPos(0)
    {
        if (!_jsn)
            throw std::bad_alloc();
        _jsn->data = this;
    }

    JSONReader::~JSONReader() {
        if (_jsn)
            jsonsl_destroy(_jsn);
    }

    bool JSONReader::writeJSON(slice json) {
        _input = json;
        _error = 0;
        _errorPos = 0;

        _jsn->data = this;
        _jsn->action_callback_PUSH = writePushCallback;
        _jsn->action_callback_POP  = writePopCallback;
        _jsn->error_callback = errorCallback;
        jsonsl_enable_all_callbacks(_jsn);

        jsonsl_feed(_jsn, (char*)json.buf, json.size);
        jsonsl_reset(_jsn);
        return true;
    }
    
    void JSONReader::writePushCallback(jsonsl_t jsn,
                                       jsonsl_action_t action,
                                       struct jsonsl_state_st *state,
                                       const char *buf)
    {
        auto self = (JSONReader*)jsn->data;
        self->push(state);
    }

    void JSONReader::writePopCallback(jsonsl_t jsn,
                                      jsonsl_action_t action,
                                      struct jsonsl_state_st *state,
                                      const char *buf)
    {
        auto self = (JSONReader*)jsn->data;
        self->pop(state);
    }

    void JSONReader::push(struct jsonsl_state_st *state) {
        switch (state->type) {
            case JSONSL_T_LIST:
                _encoder.beginArray();
                break;
            case JSONSL_T_OBJECT:
                _encoder.beginDictionary();
                break;
        }
    }

    void JSONReader::pop(struct jsonsl_state_st *state) {
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
                    char *dst = buf;
                    auto end = str.end();
                    for (auto src = (const char*)str.buf; src < end; ++src) {
                        char c = *src;
                        if (c != '\\') {
                            *dst++ = c;
                        } else {
                            switch (*++src) {
                                case 'n':   *dst++ = '\n';  break;
                                case 'r':   *dst++ = '\r';  break;
                                case 't':   *dst++ = '\t';  break;
                                case 'b':   *dst++ = '\b';  break;
                                case 'u': {
                                    //TODO: Parse Unicode escape
                                    break;
                                }
                                default:    *dst++ = *src;
                            }
                        }
                    }
                    str = slice(buf, dst);
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

    int JSONReader::errorCallback(jsonsl_t jsn,
                                  jsonsl_error_t err,
                                  struct jsonsl_state_st *state,
                                  char *errat)
    {
        auto self = (JSONReader*)jsn->data;
        self->_error = err;
        self->_errorPos = errat - (char*)self->_input.buf;
        jsonsl_stop(jsn);
        return 0;
    }

}
