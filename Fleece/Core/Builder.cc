//
// Builder.cc
//
// Copyright © 2021 Couchbase. All rights reserved.
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

#include "Builder.hh"
#include "FleeceException.hh"
#include "FleeceImpl.hh"
#include "JSONEncoder.hh"
#include "JSLexer.hh"
#include "JSON5.hh"
#include "MutableDict.hh"
#include "MutableArray.hh"
#include "NumConversion.hh"
#include "slice_stream.hh"
#include <ctype.h>
#include <sstream>

#ifdef __APPLE__
#include "fleece/Fleece+CoreFoundation.h"
#endif

namespace fleece::impl::builder {
    using namespace std;


    class Builder : private JSLexer {
    public:
        Builder(slice formatString, va_list args)
        :JSLexer(formatString, args)
        { }


        // Parses the format, interpolates args, and returns a new mutable Array or Dict.
        RetainedConst<Value> buildValue() {
            switch (peekToken()) {
                case '[': {
                    Retained<MutableArray> array = MutableArray::newArray();
                    _buildInto(array);
                    finished();
                    return array.get();
                }
                case '{': {
                    Retained<MutableDict> dict = MutableDict::newDict();
                    _buildInto(dict);
                    finished();
                    return dict.get();
                }
                default:
                    fail("only '{...}' or '[...]' allowed at top level");
            }
        }


        void buildInto(MutableDict *dict) {
            peekToken('{', "expected '{'");
            _buildInto(dict);
        }


        void buildInto(MutableArray *array) {
            peekToken('[', "expected '['");
            _buildInto(array);
        }

    protected:
        // Parses a Fleece value from the input and stores it in the ValueSlot.
        // Recognizes a '%' specifier, and calls `putParameter` to read the value from the args.
        bool _buildValue(ValueSlot &inSlot) {
            switch (peekValue()) {
                case ValueType::array: {
                    Retained<MutableArray> array = MutableArray::newArray();
                    _buildInto(array);
                    inSlot.set(array);
                    break;
                }
                case ValueType::dict: {
                    Retained<MutableDict> dict = MutableDict::newDict();
                    _buildInto(dict);
                    inSlot.set(dict);
                    break;
                }
                case ValueType::null:
                    readIdentifier("null");
                    inSlot.set(nullValue);
                    break;
                case ValueType::boolean_true:
                    readIdentifier("true");
                    inSlot.set(true);
                    break;
                case ValueType::boolean_false:
                    readIdentifier("false");
                    inSlot.set(false);
                    break;
                case ValueType::number:
                    std::visit([&](auto n) {inSlot.set(n);}, readNumber());
                    break;
                case ValueType::string:
                    inSlot.set(readString());
                    break;
                case ValueType::arg:
                    getChar();
                    return putParameter(inSlot);
                case ValueType::error:
                    fail("invalid start of value");
            }
            return true;
        }


        // Parses a JSON5 object from the input and adds its entries to `dict`.
        void _buildInto(MutableDict *dict) {
            getChar();      // skip the opening '{' *without verifying*
            while (peekToken() != '}') {
                string key = readKey();
                if (!_buildValue(dict->setting(key)))
                    dict->remove(key);

                if (peekToken() == ',')         // Note: JSON5 allows trailing `,` before `}`
                    getChar();
                else
                    peekToken('}', "unexpected token after dict item");
            }
            getChar(); // eat close bracket/brace
        }


        // Parses a JSON5 array from the input and adds its entries to `dict`.
        void _buildInto(MutableArray *array) {
            getChar();      // skip the opening '[' *without verifying*
            while (peekToken() != ']') {
                if (!_buildValue(array->appending()))
                    array->remove(array->count() - 1, 1);

                if (peekToken() == ',')         // Note: JSON5 allows trailing `,` before `]`
                    getChar();
                else
                    peekToken(']', "unexpected token after array item");
            }
            getChar(); // eat close bracket/brace
        }


        // This is where those crazy printf format specs get parsed.
        bool putParameter(ValueSlot &inSlot) {
            auto p = readArg();
            if (std::holds_alternative<monostate>(p))
                return false;
#ifdef __APPLE__
            if (std::holds_alternative<const void*>(p)) {
                FLSlot_SetCFValue(FLSlot(&inSlot), CFTypeRef(std::get<const void*>(p)));
                return true;
            }
#endif
            std::visit([&](auto val) {
                if constexpr (!is_same_v<decltype(val), monostate> && !is_same_v<decltype(val), const void*>)
                    inSlot.set(val);
            }, p);
            return true;
        }

    };


#pragma mark - ENCODER:


    template <class ENCODER>
    class Buildencoder : private JSLexer {
    public:

        Buildencoder(ENCODER& encoder, slice formatString, va_list args)
        :JSLexer(formatString, args)
        ,_encoder(encoder)
        { }


        // Parses the format, interpolates args, and returns a new mutable Array or Dict.
        void buildValue() {
            switch (peekToken()) {
                case '[':
                    writeArray();
                    break;
                case '{':
                    writeDict();
                    break;
                default:
                    writeDictInterior();
                    break;
            }
            finished();
        }

    protected:
        // Parses a Fleece value from the input and writes it, prefixed by the key if one's given.
        // Recognizes a '%' specifier, and calls `writeParameter` to read the value from the args.
        bool writeValue(slice key = nullslice) {
            auto v = peekValue();
            if (v != ValueType::arg && key)
                _encoder.writeKey(key);
            switch (v) {
                case ValueType::array:
                    writeArray();
                    break;
                case ValueType::dict:
                    writeDict();
                    break;
                case ValueType::null:
                    readIdentifier("null");
                    _encoder.writeNull();
                    break;
                case ValueType::boolean_true:
                    readIdentifier("true");
                    _encoder.writeBool(true);
                    break;
                case ValueType::boolean_false:
                    readIdentifier("false");
                    _encoder.writeBool(false);
                    break;
                case ValueType::number:
                    std::visit([&](auto n) {_encoder << n;}, readNumber());
                    break;
                case ValueType::string:
                    _encoder.writeString(readString());
                    break;
                case ValueType::arg:
                    getChar();
                    return writeParameter(key);
                default:
                    fail("invalid start of value");
            }
            return true;
        }


        // Parses a JSON5 object from the input and adds its entries to `dict`.
        void writeDict() {
            getChar();      // skip the opening '{' *without verifying*
            _encoder.beginDictionary();
            if (peekToken() != '}')
                writeDictInterior();
            else
                getChar();
            _encoder.endDictionary();
            peekToken('}', "unexpected token after dict item");
            getChar(); // eat close bracket/brace
        }


        void writeDictInterior() {
            while (true) {
                writeValue(readKey());

                if (peekToken() != ',')         // Note: JSON5 allows trailing `,` before `}`
                    break;
                getChar();
                if (char c = peekToken(); c == '}' || c == '\0')
                    break;
            }
        }


        // Parses a JSON5 array from the input and adds its entries to `dict`.
        void writeArray() {
            getChar();      // skip the opening '[' *without verifying*
            _encoder.beginArray();
            while (peekToken() != ']') {
                writeValue();
                if (peekToken() == ',')         // Note: JSON5 allows trailing `,` before `]`
                    getChar();
                else
                    peekToken(']', "unexpected token after array item");
            }
            getChar(); // eat close bracket/brace
            _encoder.endArray();
        }


        // This is where those crazy printf format specs get parsed.
        // A parameter may be skipped, if the format specifier has a `-` prefix and the value is 0/false/null.
        // If a key is given it will be written before the value, unless the parameter is skipped.
        bool writeParameter(slice key = nullslice) {
            auto p = readArg();
            if (std::holds_alternative<monostate>(p))
                return false;
            if (key)
                _encoder.writeKey(key);
            std::visit([&](auto val) {
                if constexpr (is_same_v<decltype(val), monostate>) {
                    //
                } else if constexpr (is_same_v<decltype(val), bool>) {
                    _encoder.writeBool(val);
                } else if constexpr (is_same_v<decltype(val), const void*>) {
#ifdef __APPLE__
                    _encoder.writeCF(val);
#endif
                } else {
                    _encoder << val;
                }
            }, p);
            return true;
        }

    private:
        ENCODER& _encoder;   // The Encoder or JSONEncoder I'm writing to
    };


#pragma mark - PUBLIC API:


    RetainedConst<Value> VBuild(const char *format, va_list args) {
        return Builder(format, args).buildValue();
    }

    RetainedConst<Value> VBuild(slice format, va_list args) {
        return Builder(format, args).buildValue();
    }


    void VEncode(Encoder& encoder, slice format, va_list args) {
        Buildencoder<Encoder>(encoder, format, args).buildValue();
    }

    void VEncode(JSONEncoder& encoder, slice format, va_list args) {
        Buildencoder<JSONEncoder>(encoder, format, args).buildValue();
    }


    RetainedConst<Value> Build(const char *format, ...) {
        va_list args;
        va_start(args, format);
        auto result = VBuild(format, args);
        va_end(args);
        return result;
    }


    void Encode(Encoder& encoder, const char *format, ...) {
        va_list args;
        va_start(args, format);
        VEncode(encoder, format, args);
        va_end(args);
    }


#ifdef __APPLE__
    RetainedConst<Value> BuildCF(CFStringRef cfFormat, ...) {
        va_list args;
        va_start(args, cfFormat);
        auto result = VBuild(nsstring_slice(cfFormat), args);
        va_end(args);
        return result;
    }

    void EncodeCF(Encoder& encoder, CFStringRef cfFormat, ...) {
        va_list args;
        va_start(args, cfFormat);
        VEncode(encoder, nsstring_slice(cfFormat), args);
        va_end(args);
    }
#endif


    void Put(MutableArray *array, const char *format, ...) {
        va_list args;
        va_start(args, format);
        Builder(format, args).buildInto(array);
        va_end(args);
    }


    void Put(MutableDict *dict, const char *format, ...) {
        va_list args;
        va_start(args, format);
        Builder(format, args).buildInto(dict);
        va_end(args);
    }

    void VPut(Value *v, const char *format, va_list args) {
        Builder builder(format, args);
        if (const Dict *dict = v->asDict()) {
            MutableDict *mutableDict = dict->asMutable();
            assert(mutableDict);
            builder.buildInto(mutableDict);
        } else {
            MutableArray *mutableArray = v->asArray()->asMutable();
            assert(mutableArray);
            builder.buildInto(mutableArray);
        }
    }

}
