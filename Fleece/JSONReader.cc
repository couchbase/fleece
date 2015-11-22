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

    JSONReader::JSONReader(class encoder &e)
    :_jsn(jsonsl_new(0x2000)),
     _encoder(e),
     _error(0),
     _errorPos(0)
    {
        if (!_jsn)
            throw std::bad_alloc();
        _jsn->data = this;
        _encoders.push_back(&e);
    }

    JSONReader::~JSONReader() {
        if (_jsn)
            jsonsl_destroy(_jsn);
    }

    bool JSONReader::writeJSON(slice json) {
        _input = json;
        _error = 0;
        _errorPos = 0;

        if (!countJSONItems(json))
            throw "JSON parse error";

        _jsn->data = this;
        _jsn->action_callback = writeCallback;
        _jsn->error_callback = NULL;
        jsonsl_enable_all_callbacks(_jsn);

        jsonsl_feed(_jsn, (char*)json.buf, json.size);
        jsonsl_reset(_jsn);
        _startToLength.clear();
        return true;
    }
    
    void JSONReader::writeCallback(jsonsl_t jsn,
                                   jsonsl_action_t action,
                                   struct jsonsl_state_st *state,
                                   const char *buf)
    {
        auto self = (JSONReader*)jsn->data;
        if (action == JSONSL_ACTION_PUSH)
            self->push(state);
        else
            self->pop(state);
    }

    void JSONReader::push(struct jsonsl_state_st *state) {
        encoder &e = *_encoders.back();
        switch (state->type) {
            case JSONSL_T_LIST:
            case JSONSL_T_OBJECT:
                slice input = _input;
                input.moveStart(state->pos_begin);
                auto count = _startToLength[state->pos_begin];
                _encoders.push_back(new encoder(e,
                                                (state->type == JSONSL_T_LIST ? kArray : kDict),
                                                (uint32_t)count, false));
                //TODO: use wide when needed
                break;
        }
    }

    void JSONReader::pop(struct jsonsl_state_st *state) {
        encoder &e = *_encoders.back();
        switch (state->type) {
            case JSONSL_T_SPECIAL: {
                unsigned f = state->special_flags;
                if (f & JSONSL_SPECIALf_NULL) {
                    e.writeNull();
                } else if (f & JSONSL_SPECIALf_BOOLEAN) {
                    e.writeBool( (f & JSONSL_SPECIALf_TRUE) != 0);
                } else if (f & JSONSL_SPECIALf_FLOAT) {
                    char *start = (char*)&_input[state->pos_begin];
                    char *end;
                    double n = ::strtod(start, &end);
                    e.writeDouble(n);
                } else if (f & JSONSL_SPECIALf_UNSIGNED) {
                    e.writeUInt(state->nelem);
                } else if (f & JSONSL_SPECIALf_SIGNED) {
                    e.writeInt(-(int64_t)state->nelem);
                }
                break;
            }
            case JSONSL_T_STRING:
            case JSONSL_T_HKEY: {
                slice str(&_input[state->pos_begin + 1],
                          state->pos_cur - state->pos_begin - 1);
                char *buf = NULL;
                if (state->nescapes > 0) {
                    // De-escape str:
                    buf = (char*)malloc(str.size);
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
                    e.writeString(str);
                else
                    e.writeKey(str);
                free(buf);
                break;
            }
            case JSONSL_T_LIST:
            case JSONSL_T_OBJECT:
                e.end();
                delete &e;
                _encoders.pop_back();
                break;
        }
    }

#pragma mark COUNTER:

    // Pre-parses the JSON to compute the count of every array/dict in it.
    bool JSONReader::countJSONItems(slice json) {
        _jsn->action_callback = countCallback;
        _jsn->error_callback = errorCallback;
        _jsn->call_OBJECT = _jsn->call_LIST = 1;
        _jsn->call_SPECIAL = _jsn->call_STRING = _jsn->call_HKEY = 0;
        jsonsl_feed(_jsn, (char*)json.buf, json.size);
        jsonsl_reset(_jsn);
        return _error == 0;
    }

    void JSONReader::countCallback(jsonsl_t jsn,
                                   jsonsl_action_t action,
                                   struct jsonsl_state_st *state,
                                   const char *buf)
    {
        if (action == JSONSL_ACTION_POP) {
            auto self = (JSONReader*)jsn->data;
            auto count = state->nelem;
            if (state->type == JSONSL_T_OBJECT)
                count /= 2;
            self->_startToLength[state->pos_begin] = count;
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
