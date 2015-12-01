//
//  Encoder+JSON.cc
//  Fleece
//
//  Created by Jens Alfke on 11/21/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//

#include "JSONConverter.hh"
#include "jsonsl.h"
#include <map>

namespace fleece {

    static bool decodeUnicodeEscape(uint8_t* &dst, const char *src);

    JSONConverter::JSONConverter(Encoder &e)
    :_jsn(jsonsl_new(0x2000)),
     _encoder(e),
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
            _error = JSONSL_ERROR_BRACKET_MISMATCH;
            _errorPos = json.size;
        }
        jsonsl_reset(_jsn);
        return (_error == JSONSL_ERROR_SUCCESS);
    }
    
    void JSONConverter::writePushCallback(jsonsl_t jsn,
                                       jsonsl_action_t action,
                                       struct jsonsl_state_st *state,
                                       const char *buf)
    {
        auto self = (JSONConverter*)jsn->data;
        self->push(state);
    }

    void JSONConverter::writePopCallback(jsonsl_t jsn,
                                      jsonsl_action_t action,
                                      struct jsonsl_state_st *state,
                                      const char *buf)
    {
        auto self = (JSONConverter*)jsn->data;
        self->pop(state);
    }

    void JSONConverter::push(struct jsonsl_state_st *state) {
        switch (state->type) {
            case JSONSL_T_LIST:
                _encoder.beginArray();
                break;
            case JSONSL_T_OBJECT:
                _encoder.beginDictionary();
                break;
        }
    }

    void JSONConverter::pop(struct jsonsl_state_st *state) {
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
                uint8_t *buf = NULL;
                bool mallocedBuf = false;
                if (state->nescapes > 0) {
                    // De-escape str:
                    mallocedBuf = str.size > 100;
                    buf = (uint8_t*)(mallocedBuf ? malloc(str.size) : alloca(str.size));
                    uint8_t *dst = buf;
                    auto end = str.end();
                    for (auto src = (const char*)str.buf; src < end; ++src) {
                        char c = *src;
                        if (__builtin_expect(c != '\\', true)) {
                            *dst++ = c;
                        } else {
                            switch (*++src) {
                                case 'n':   *dst++ = '\n';  break;
                                case 'r':   *dst++ = '\r';  break;
                                case 't':   *dst++ = '\t';  break;
                                case 'b':   *dst++ = '\b';  break;
                                case 'u':
                                    if (src + 4 >= end || !decodeUnicodeEscape(dst, src+1)) {
                                        errorCallback(_jsn, JSONSL_ERROR_UESCAPE_TOOSHORT,
                                                      state, (char*)src);
                                        return;
                                    }
                                    src += 4; break;
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

    int JSONConverter::errorCallback(jsonsl_t jsn,
                                  jsonsl_error_t err,
                                  struct jsonsl_state_st *state,
                                  char *errat)
    {
        auto self = (JSONConverter*)jsn->data;
        self->_error = err;
        self->_errorPos = errat - (char*)self->_input.buf;
        jsonsl_stop(jsn);
        return 0;
    }


#pragma mark - UTILITIES:
    
    // similar to BSD digittoint, but returns -1 on failure
    static int digit2int(char ch) {
        int d = ch - '0';
        if ((unsigned) d < 10) {
            return d;
        }
        d = ch - 'a';
        if ((unsigned) d < 6) {
            return d + 10;
        }
        d = ch - 'A';
        if ((unsigned) d < 6) {
            return d + 10;
        }
        return -1;
    }

    // Writes a Unicode code point from 0000 to FFFF as a UTF-8 byte sequence
    static void writeUTF8(uint8_t* &dst, unsigned u) {
        // https://en.wikipedia.org/wiki/UTF-8#Description
        if (u < 0x0080) {
            *dst++ = (uint8_t)u;
        } else if (u < 0x0800) {
            *dst++ = (uint8_t)(0xC0 | (u >> 6));
            *dst++ = (uint8_t)(0x80 | (u & 0x3F));
        } else {
            //assert(u < 0xFFFF);
            *dst++ = (uint8_t)(0xE0 | (u >> 12));
            *dst++ = (uint8_t)(0x80 | ((u >>  6) & 0x3F));
            *dst++ = (uint8_t)(0x80 | (u & 0x3F));
        }
    }

    static bool decodeUnicodeEscape(uint8_t* &dst, const char* src) {
        int d0 = digit2int(src[0]), d1 = digit2int(src[1]),
            d2 = digit2int(src[2]), d3 = digit2int(src[3]);
        if (d0 < 0 || d1 < 0 || d2 < 0 || d3 < 0)
            return false;
        unsigned u = (d0 << 12) | (d1 << 8) | (d2 << 4) | (d3);
        writeUTF8(dst, u);
        return true;
    }

}
