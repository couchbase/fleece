//
// JSONEncoder.hh
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

#pragma once

#include "Writer.hh"
#include "FleeceImpl.hh"
#include "FleeceException.hh"
#include "NumConversion.hh"
#include "ParseDate.hh"
#include <algorithm>
#include <stdio.h>
#include "betterassert.hh"


namespace fleece { namespace impl {

    /** Generates JSON-encoded data; this is a template so different outputs can be plugged in.
        The normal WRITER is \ref Writer; see the class \ref JSONEncoder, below. */
    template <class WRITER>
    class JSONEncoderTo {
    public:
        JSONEncoderTo()
        { }

        explicit JSONEncoderTo(WRITER &&writer)
        :_out(std::move(writer))
        { }

        /** In JSON5 mode, dictionary keys that are JavaScript identifiers are not quoted. */
        void setJSON5(bool j5)                  {_json5 = j5;}

        /** In canonical mode, \ref writeDict writes keys in ascending memcmp order, so that the
            same input always produces the same output.
            \warning Canonical mode is not compatible with writing a dict "by hand" using
                \ref beginDictionary ... \ref endDictionary. */
        void setCanonical(bool canonical)       {_canonical = canonical;}

        WRITER& writer()                        {return _out;}
        const WRITER& writer() const            {return _out;}

        /** Resets the encoder so it can be used again. */
        void reset()                            {_out.reset(); _first = true;}

        /////// Writing data:

        void writeNull()                        {comma(); _out << "null"_sl;}
        void writeBool(bool b)                  {comma(); _out << (b ? "true"_sl : "false"_sl);}

        void writeInt(int64_t i)                {_writeInt("%lld", i);}
        void writeUInt(uint64_t i)              {_writeInt("%llu", i);}
        void writeFloat(float f)                {_writeFloat(f);}
        void writeDouble(double d)              {_writeFloat(d);}

        void writeString(const std::string &s)  {writeString(slice(s));}
        inline void writeString(slice s);
        inline void writeDateString(int64_t timestamp, bool asUTC);
        
        void writeData(slice d)                 {comma(); _out << '"'; _out.writeBase64(d);
                                                          _out << '"';}
        inline void writeValue(const Value *v);

        void writeJSON(slice json)              {comma(); _out << json;}
        void writeRaw(slice raw)                {_out << raw;}

#ifdef __OBJC__
        void writeObjC(id)                      {FleeceException::_throw(JSONError,
                                                    "Encoding Obj-C to JSON is unimplemented");}
#endif

        //////// Writing arrays:

        void beginArray()                       {comma(); _out << '['; _first = true;}
        void endArray()                         {_out << ']'; _first = false;}

        //////// Writing dictionaries:

        void beginDictionary()                  {assert(!_canonical); _beginDictionary();}
        void endDictionary()                    {_out << '}'; _first = false;}

        void writeKey(slice s);
        void writeKey(const std::string &s)     {writeKey(slice(s));}
        void writeKey(const Value *v)           {writeKey(v->asString());}

        //////// "<<" convenience operators;

        // Note: overriding <<(bool) would be dangerous due to implicit conversion
        JSONEncoderTo& operator<< (long long i)           {writeInt(i); return *this;}
        JSONEncoderTo& operator<< (unsigned long long i)  {writeUInt(i); return *this;}
        JSONEncoderTo& operator<< (long i)            {writeInt(i); return *this;}
        JSONEncoderTo& operator<< (unsigned long i)   {writeUInt(i); return *this;}
        JSONEncoderTo& operator<< (int i)             {writeInt(i); return *this;}
        JSONEncoderTo& operator<< (unsigned int i)    {writeUInt(i); return *this;}
        JSONEncoderTo& operator<< (double d)          {writeDouble(d); return *this;}
        JSONEncoderTo& operator<< (float f)           {writeFloat(f); return *this;}
        JSONEncoderTo& operator<< (const std::string &str)   {writeString(str); return *this;}
        JSONEncoderTo& operator<< (slice s)           {writeString(s); return *this;} // string not data!
        JSONEncoderTo& operator<< (const Value *v)    {writeValue(v); return *this;}

        // Just for API compatibility with Encoder class:
        void beginArray(size_t)                       {beginArray();}
        void beginDictionary(size_t)                  {beginDictionary();}
        void writeUndefined() {
            FleeceException::_throw(JSONError, "Cannot write `undefined` to JSON encoder");
        }

    private:
        void _beginDictionary()                 {comma(); _out << '{'; _first = true;}
        inline void writeDict(const Dict*);
        
        void comma() {
            if (_first)
                _first = false;
            else
                _out << ',';
        }

        template <class T>
        void _writeInt(const char *fmt, T t) {
            comma();
            char str[32];
            _out << slice(str, sprintf(str, fmt, t));
        }

        template <class T>
        void _writeFloat(T t) {
            comma();
            char str[32];
            _out << slice(str, WriteFloat(t, str, sizeof(str)));
        }

        WRITER _out;
        bool _json5 {false};
        bool _canonical {false};
        bool _first {true};
    };


#pragma mark - METHOD IMPLEMENTATIONS


    template <class WRITER>
    inline void JSONEncoderTo<WRITER>::writeString(slice str) {
        comma();
        _out << '"';
        auto start = (const uint8_t*)str.buf;
        auto end = (const uint8_t*)str.end();
        for (auto p = start; p < end; p++) {
            uint8_t ch = *p;
            if (ch == '"' || ch == '\\' || ch < 32 || ch == 127) {
                // Write characters from start up to p-1:
                _out << slice(start, p);
                start = p + 1;
                switch (ch) {
                    case '"':
                    case '\\':
                        _out << '\\';
                        --start; // ch will be written in next pass
                        break;
                    case '\r':
                        _out << "\\r"_sl;
                        break;
                    case '\n':
                        _out << "\\n"_sl;
                        break;
                    case '\t':
                        _out << "\\t"_sl;
                        break;
                    default: {
                        char buf[7];
                        _out << slice(buf, sprintf(buf, "\\u%04x", (unsigned)ch));
                        break;
                    }
                }
            }
        }
        if (end > start)
            _out << slice(start, end);
        _out << '"';
    }


    template <class WRITER>
    inline void JSONEncoderTo<WRITER>::writeDateString(int64_t timestamp, bool asUTC) {
        char str[kFormattedISO8601DateMaxSize];
        writeString(FormatISO8601Date(str, timestamp, asUTC));
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

    template <class WRITER>
    inline void JSONEncoderTo<WRITER>::writeKey(slice s) {
        assert(s);
        if (_json5 && canBeUnquotedJSON5Key(s)) {
            comma();
            _out << s;
        } else {
            writeString(s);
        }
        _out << ':';
        _first = true;
    }


    template <class WRITER>
    inline void JSONEncoderTo<WRITER>::writeDict(const Dict *dict) {
        auto count = dict->count();
        _beginDictionary();
        if (_canonical && count > 1) {
            // In canonical mode, ensure the keys are written in sorted order:
            struct kv {
                slice key;
                const Value *value;
                bool operator< (const kv &other) const {return key < other.key;}
            };
            smallVector<kv, 4> items;
            items.reserve(count);
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


    template <class WRITER>
    inline void JSONEncoderTo<WRITER>::writeValue(const Value *v) {
        switch (v->type()) {
            case kNull:
                if (v->isUndefined()) {
                    comma();
                    _out << "undefined"_sl;
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


#pragma mark - JSONENCODER:


    /** The default JSON encoder that writes to memory producing an alloc_slice. */
    class JSONEncoder : public JSONEncoderTo<Writer> {
    public:
        explicit JSONEncoder(size_t initialOutputCapacity =256)
        :JSONEncoderTo<Writer>(Writer(initialOutputCapacity))
        { }

        bool isEmpty() const                    {return writer().length() == 0;}
        size_t bytesWritten() const             {return writer().length();}

        /** Returns the encoded data. */
        alloc_slice finish()                    {return writer().finish();}
    };

} }
