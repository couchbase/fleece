//
//  JSONEncoder.cc
//  Fleece
//
//  Created by Jens Alfke on 2/14/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "JSONEncoder.hh"

namespace fleece {

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


    void JSONEncoder::writeValue(const Value *v, const SharedKeys *sk) {
        switch (v->type()) {
            case kNull:
                writeNull();
                return;
            case kBoolean:
                writeBool(v->asBool());
                return;
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
                return;
            case kString:
                writeString(v->asString());
                return;
            case kData:
                writeData(v->asData());
                return;
            case kArray:
                beginArray();
                for (auto iter = v->asArray()->begin(); iter; ++iter)
                    writeValue(iter.value(), sk);
                endArray();
                return;
            case kDict:
                beginDictionary();
                for (auto iter = v->asDict()->begin(sk); iter; ++iter) {
                    slice keyStr = iter.keyString();
                    if (keyStr) {
                        writeKey(keyStr);
                    } else {
                        // non-string keys are possible...
                        comma();
                        _first = true;
                        writeValue(iter.key(), sk);
                        _out << ':';
                        _first = true;
                    }
                    writeValue(iter.value(), sk);
                }
                endDictionary();
                return;
            default:
                FleeceException::_throw(UnknownValue, "illegal typecode in Value; corrupt data?");
        }
    }

}
