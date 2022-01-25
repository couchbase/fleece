//
// JSONEncoder.hh
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once

#include "Writer.hh"
#include "Value.hh"
#include "FleeceException.hh"
#include "NumConversion.hh"
#include <stdio.h>


namespace fleece { namespace impl {

    /** Generates JSON-encoded data. */
    class JSONEncoder {
    public:
        JSONEncoder(size_t reserveOutputSize =256)
        :_out(reserveOutputSize)
        { }

        /** In JSON5 mode, dictionary keys that are JavaScript identifiers will be unquoted. */
        void setJSON5(bool j5)                  {_json5 = j5;}
        void setCanonical(bool canonical)       {_canonical = canonical;}

        bool isEmpty() const                    {return _out.length() == 0;}
        size_t bytesWritten() const             {return _out.length();}

        /** Returns the encoded data. */
        alloc_slice finish()                    {return _out.finish();}

        /** Resets the encoder so it can be used again. */
        void reset()                            {_out.reset(); _first = true;}

        /////// Writing data:

        void writeNull()                        {comma(); _out << slice("null");}
        void writeBool(bool b)                  {comma(); _out.write(b ? "true"_sl : "false"_sl);}

        void writeInt(int64_t i)                {_writeInt("%lld", i);}
        void writeUInt(uint64_t i)              {_writeInt("%llu", i);}
        void writeFloat(float f)                {_writeFloat(f);}
        void writeDouble(double d)              {_writeFloat(d);}

        void writeString(const std::string &s)  {writeString(slice(s));}
        void writeString(slice s);
        void writeDateString(int64_t timestamp, bool asUTC);
        
        void writeData(slice d)                 {comma(); _out << '"'; _out.writeBase64(d);
                                                          _out << '"';}
        void writeValue(const Value *v);

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

        void beginDictionary()                  {comma(); _out << '{'; _first = true;}
        void endDictionary()                    {_out << '}'; _first = false;}

        void writeKey(slice s);
        void writeKey(const std::string &s)     {writeKey(slice(s));}
        void writeKey(const Value *v)           {writeKey(v->asString());}

        //////// "<<" convenience operators;

        // Note: overriding <<(bool) would be dangerous due to implicit conversion
        JSONEncoder& operator<< (long long i)           {writeInt(i); return *this;}
        JSONEncoder& operator<< (unsigned long long i)  {writeUInt(i); return *this;}
        JSONEncoder& operator<< (long i)            {writeInt(i); return *this;}
        JSONEncoder& operator<< (unsigned long i)   {writeUInt(i); return *this;}
        JSONEncoder& operator<< (int i)             {writeInt(i); return *this;}
        JSONEncoder& operator<< (unsigned int i)    {writeUInt(i); return *this;}
        JSONEncoder& operator<< (double d)          {writeDouble(d); return *this;}
        JSONEncoder& operator<< (float f)           {writeFloat(f); return *this;}
        JSONEncoder& operator<< (const std::string &str)   {writeString(str); return *this;}
        JSONEncoder& operator<< (slice s)           {writeString(s); return *this;} // string not data!
        JSONEncoder& operator<< (const Value *v)    {writeValue(v); return *this;}

        // Just for API compatibility with Encoder class:
        void beginArray(size_t)                       {beginArray();}
        void beginDictionary(size_t)                  {beginDictionary();}
        void writeUndefined() {
            FleeceException::_throw(JSONError, "Cannot write `undefined` to JSON encoder");
        }

    private:
        void writeDict(const Dict*);
        
        void comma() {
            if (_first)
                _first = false;
            else
                _out << ',';
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

        template <class T>
        void _writeInt(const char *fmt, T t) {
            comma();
            char str[32];
            _out.write(str, sprintf(str, fmt, t));
        }

#pragma GCC diagnostic pop

        template <class T>
        void _writeFloat(T t) {
            comma();
            char str[32];
            _out.write(str, WriteFloat(t, str, sizeof(str)));
        }

        Writer _out;
        bool _json5 {false};
        bool _canonical {false};
        bool _first {true};
    };

} }
