//
// JSONConverter.cc
//
// Copyright (c) 2015 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "JSONConverter.hh"
#include "jsonsl.h"
#include <map>

namespace fleece { namespace impl {

    static int errorCallback(struct jsonsl_st * jsn,
                             jsonsl_error_t err,
                             struct jsonsl_state_st *state,
                             char *errat) noexcept;
    static void writePushCallback(struct jsonsl_st * jsn,
                                  jsonsl_action_t action,
                                  struct jsonsl_state_st *state,
                                  const char *buf) noexcept;
    static void writePopCallback(struct jsonsl_st * jsn,
                                 jsonsl_action_t action,
                                 struct jsonsl_state_st *state,
                                 const char *buf) noexcept;

    JSONConverter::JSONConverter(Encoder &e) noexcept
    :_encoder(e),
     _jsn(jsonsl_new(50)),      // never returns nullptr, according to source code
     _jsonError(JSONSL_ERROR_SUCCESS),
     _errorPos(0)
    {
        _jsn->data = this;
    }

    JSONConverter::~JSONConverter() {
        jsonsl_destroy(_jsn);
    }

    void JSONConverter::reset() {
        jsonsl_reset(_jsn);
        _jsonError = JSONSL_ERROR_SUCCESS;
        _errorPos = 0;
    }

    const char* JSONConverter::errorMessage() noexcept {
        if (!_errorMessage.empty())
            return _errorMessage.c_str();
        else if (_jsonError == kErrExceptionThrown)
            return "Unexpected C++ exception";
        else if (_jsonError == kErrTruncatedJSON)
            return "Truncated JSON";
        else
            return jsonsl_strerror((jsonsl_error_t)_jsonError);
    }


    bool JSONConverter::encodeJSON(slice json) {
        _input = json;
        _errorMessage.clear();
        _errorCode = NoError;
        _jsonError = JSONSL_ERROR_SUCCESS;
        _errorPos = 0;

        _jsn->data = this;
        _jsn->action_callback_PUSH = writePushCallback;
        _jsn->action_callback_POP  = writePopCallback;
        _jsn->error_callback = errorCallback;
        jsonsl_enable_all_callbacks(_jsn);

        jsonsl_feed(_jsn, (char*)json.buf, json.size);
        if (_jsn->level > 0 && !_jsonError) {
            // Input is valid JSON so far, but truncated:
            _jsonError = kErrTruncatedJSON;
            _errorPos = json.size;
        }
        jsonsl_reset(_jsn);
        return (_jsonError == JSONSL_ERROR_SUCCESS);
    }

    /*static*/ alloc_slice JSONConverter::convertJSON(slice json, SharedKeys *sk) {
        Encoder enc;
        enc.setSharedKeys(sk);
        JSONConverter cvt(enc);
        throwIf(!cvt.encodeJSON(slice(json)), JSONError, cvt.errorMessage());
        return enc.finish();
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
                char *buf = nullptr;
                bool mallocedBuf = false;
                if (state->nescapes > 0) {
                    // De-escape str:
                    mallocedBuf = str.size > 100;
                    buf = (char*)(mallocedBuf ? malloc(str.size) : alloca(str.size));
                    jsonsl_error_t err = JSONSL_ERROR_SUCCESS;
                    const char *errat;
                    auto size = jsonsl_util_unescape_ex((const char*)str.buf, buf, str.size,
                                                        nullptr, nullptr, &err, &errat);
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

    int JSONConverter::gotError(int err, size_t pos) noexcept {
        _jsonError = err;
        _errorPos = pos;
        _errorCode = JSONError;
        jsonsl_stop(_jsn);
        return 0;
    }

    int JSONConverter::gotError(int err, const char *errat) noexcept {
        return gotError(err, errat ? (errat - (char*)_input.buf) : 0);
    }

    void JSONConverter::gotException(ErrorCode code, const char *what, size_t pos) noexcept {
        gotError(kErrExceptionThrown, pos);
        _errorCode = code;
        _errorMessage = what;
    }

    // Callbacks:

    static inline JSONConverter* converter(jsonsl_t jsn) noexcept {
        return (JSONConverter*)jsn->data;
    }
    
    static void writePushCallback(jsonsl_t jsn,
                                  jsonsl_action_t action,
                                  struct jsonsl_state_st *state,
                                  const char *buf) noexcept
    {
        try {
            converter(jsn)->push(state);
        } catch (const FleeceException &x) {
            converter(jsn)->gotException(x.code, x.what(), state->pos_begin);
        } catch (...) {
            converter(jsn)->gotException(InternalError, nullptr, state->pos_begin);
        }
    }

    static void writePopCallback(jsonsl_t jsn,
                                 jsonsl_action_t action,
                                 struct jsonsl_state_st *state,
                                 const char *buf) noexcept
    {
        try {
            converter(jsn)->pop(state);
        } catch (const FleeceException &x) {
            converter(jsn)->gotException(x.code, x.what(), state->pos_begin);
        } catch (...) {
            converter(jsn)->gotException(InternalError, nullptr, state->pos_begin);
        }
    }

    static int errorCallback(jsonsl_t jsn,
                             jsonsl_error_t err,
                             struct jsonsl_state_st *state,
                             char *errat) noexcept
    {
        return converter(jsn)->gotError(err, errat);
    }

} }
