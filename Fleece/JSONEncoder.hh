//
//  JSONEncoder.hh
//  Fleece
//
//  Created by Jens Alfke on 2/14/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once

#include "Writer.hh"
#include "FleeceException.hh"


namespace fleece {

    class SharedKeys;

    /** Generates JSON-encoded data. */
    class JSONEncoder {
    public:
        JSONEncoder(size_t reserveOutputSize =256)
        :_out(reserveOutputSize)
        { }

        /** In JSON5 mode, dictionary keys that are JavaScript identifiers will be unquoted. */
        void setJSON5(bool j5)                  {_json5 = j5;}

        bool isEmpty() const                    {return _out.length() == 0;}
        size_t bytesWritten() const             {return _out.length();}

        /** Returns the encoded data. */
        alloc_slice extractOutput()             {return _out.extractOutput();}

        /** Resets the encoder so it can be used again. */
        void reset()                            {_out.reset(); _first = true;}

        /** Associates a SharedKeys object with this Encoder, for use by writeValue(). */
        void setSharedKeys(SharedKeys *s)       {_sharedKeys = s;}

        /////// Writing data:

        void writeNull()                        {comma(); _out << slice("null");}
        void writeBool(bool b)                  {comma(); _out.write(b ? "true"_sl : "false"_sl);}

        void writeInt(int64_t i)                {writef("%lld", i);}
        void writeUInt(uint64_t i)              {writef("%llu", i);}
        void writeFloat(float f)                {writef("%.6g", f);}
        void writeDouble(double d)              {writef("%.16g", d);}

        void writeString(const std::string &s)  {writeString(slice(s));}
        void writeString(slice s);
        
        void writeData(slice d)                 {comma(); _out << '"'; _out.writeBase64(d);
                                                          _out << '"';}
        void writeValue(const Value*);
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
        void beginDictionary(size_t)                  {beginArray();}

    private:
        void comma() {
            if (_first)
                _first = false;
            else
                _out << ',';
        }

        template <class T>
        void writef(const char *fmt, T t) {
            comma();
            char str[32];
            _out.write(str, sprintf(str, fmt, t));
        }

        Writer _out;
        bool _json5 {false};
        bool _first {true};
        SharedKeys *_sharedKeys {nullptr};
    };

}
