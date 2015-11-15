//
//  Encoder.hh
//  Fleece
//
//  Created by Jens Alfke on 1/26/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#ifndef __Fleece__DataWriter__
#define __Fleece__DataWriter__

#include "Value.hh"
#include "Writer.hh"
#include <ctime>
#include <iostream>
#include <unordered_map>


namespace fleece {

    class encoder {
    public:
        encoder(Writer&);

        void reset();

        void writeNull();
        void writeBool (bool);

        void writeInt(int64_t i)                {writeInt(i, false);}
        void writeUInt(uint64_t i)              {writeInt((int64_t)i, true);}
        void writeFloat(float);
        void writeDouble(double);

        void writeString(std::string);
        void writeString(slice s);
        void writeData(slice s);

        encoder writeArray(uint32_t count)      {return writeArrayOrDict(value::kArrayTag, count);}
        encoder writeDict(uint32_t count)       {return writeArrayOrDict(value::kDictTag, count);}

        void writeKey(std::string);
        void writeKey(slice);

        // Note: overriding <<(bool) would be dangerous due to implicit conversion
        encoder& operator<< (int64_t i)         {writeInt(i); return *this;}
        encoder& operator<< (double d)          {writeDouble(d); return *this;}
        encoder& operator<< (float f)           {writeFloat(f); return *this;}
        encoder& operator<< (std::string str)   {writeString(str); return *this;}
        encoder& operator<< (slice s)           {writeString(s); return *this;} // string not data!

        void end();
        
#ifdef __OBJC__
        void write(id);
#endif

    private:
        encoder(encoder *parent, size_t offset, size_t keyOffset, size_t count);
        void writeValue(value::tags, uint8_t *buf, size_t size, bool canInline =true);
        void writeSpecial(uint8_t special);
        void writeInt(int64_t i, bool isUnsigned);
        void writeData(value::tags, slice s);
        encoder writeArrayOrDict(value::tags, uint32_t count);

        encoder *_parent;       // Encoder that created this one
        size_t _offset;         // Offset in _out to write next inline value
        size_t _keyOffset;      // Offset in _out to write next dictionary key
        size_t _count;          // Count of collection I'm adding to
        Writer& _out;           // Where output is written to
        bool _blockedOnKey :1;  // True if writes should be refused
        bool _writingKey :1;    // True if value being written is a key
    };


}

#endif /* defined(__Fleece__DataWriter__) */
