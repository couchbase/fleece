//
// JSONEncoder.cc
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
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

#include "JSONEncoder.hh"
#include "FleeceImpl.hh"
#include <algorithm>

namespace fleece { namespace impl {

    void JSONEncoder::writeString(slice str) {
        comma();
        _out << '"';
        auto start = (const uint8_t*)str.buf;
        auto end = (const uint8_t*)str.end();
        for (auto p = start; p < end; p++) {
            uint8_t ch = *p;
            if (ch == '"' || ch == '\\' || ch < 32 || ch == 127) {
                // Write characters from start up to p-1:
                _out.write({start, p});
                start = p + 1;
                switch (ch) {
                    case '"':
                    case '\\':
                        _out << '\\';
                        --start; // ch will be written in next pass
                        break;
                    case '\r':
                        _out.write("\\r"_sl);
                        break;
                    case '\n':
                        _out.write("\\n"_sl);
                        break;
                    case '\t':
                        _out.write("\\t"_sl);
                        break;
                    default: {
                        char buf[7];
                        _out.write(buf, sprintf(buf, "\\u%04x", (unsigned)ch));
                        break;
                    }
                }
            }
        }
        _out.write({start, end});
        _out << '"';
    }


    static inline bool canBeUnquotedJSON5Key(slice key) {
        if (key.size == 0 || isdigit(key[0]))
            return false;
        for (unsigned i = 0; i < key.size; i++) {
            if (!isalnum(key[i]) && key[i] != '_' && key[i] != '$')
                return false;
        }
        return true;
    }

    void JSONEncoder::writeKey(slice s) {
        if (_json5 && canBeUnquotedJSON5Key(s)) {
            comma();
            _out.write((char*)s.buf, s.size);
        } else {
            writeString(s);
        }
        _out << ':';
        _first = true;
    }


    void JSONEncoder::writeDict(const Dict *dict) {
        beginDictionary();
        if (_canonical) {
            // In canonical mode, ensure the keys are written in sorted order:
            struct kv {
                slice key;
                const Value *value;
                bool operator< (const kv &other) const {return key < other.key;}
            };
            std::vector<kv> items;
            items.reserve(dict->count());
            for (auto iter = dict->begin(); iter; ++iter)
                items.push_back({iter.keyString(), iter.value()});
            std::sort(items.begin(), items.end());
            for (auto &item : items) {
                writeKey(item.key);
                writeValue(item.value);
            }
        } else {
            for (auto iter = dict->begin(); iter; ++iter) {
                slice keyStr = iter.keyString();
                if (keyStr) {
                    writeKey(keyStr);
                } else {
                    // non-string keys are possible...
                    comma();
                    _first = true;
                    writeValue(iter.key());
                    _out << ':';
                    _first = true;
                }
                writeValue(iter.value());
            }
        }
        endDictionary();
    }


    void JSONEncoder::writeValue(const Value *v) {
        switch (v->type()) {
            case kNull:
                if (v->isUndefined()) {
                    comma();
                    _out << slice("undefined");
                } else {
                    writeNull();
                }
                break;
            case kBoolean:
                writeBool(v->asBool());
                break;
            case kNumber:
                if (v->isInteger()) {
                    auto i = v->asInt();
                    if (v->isUnsigned())
                        writeUInt(i);
                    else
                        writeInt(i);
                } else if (v->isDouble()) {
                    writeDouble(v->asDouble());
                } else {
                    writeFloat(v->asFloat());
                }
                break;
            case kString:
                writeString(v->asString());
                break;
            case kData:
                writeData(v->asData());
                break;
            case kArray:
                beginArray();
                for (auto iter = v->asArray()->begin(); iter; ++iter)
                    writeValue(iter.value());
                endArray();
                break;
            case kDict:
                writeDict(v->asDict());
                break;
            default:
                FleeceException::_throw(UnknownValue, "illegal typecode in Value; corrupt data?");
        }
    }

} }
