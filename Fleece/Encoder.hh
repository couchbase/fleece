//
//  Encoder.hh
//  Fleece
//
//  Created by Jens Alfke on 1/26/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#ifndef Fleece_Encoder_hh
#define Fleece_Encoder_hh

#include "Value.hh"
#include "Writer.hh"
#include <ctime>
#include <iostream>
#include <unordered_map>


namespace fleece {

    class encoder {
    public:
        encoder(Writer&);
        encoder(encoder &parent, valueType, uint32_t count, bool wide =false);
        encoder(encoder&&);
        ~encoder();

        void reset();

        void writeNull();
        void writeBool (bool);

        void writeInt(int64_t i);
        void writeUInt(uint64_t i);
        void writeFloat(float);
        void writeDouble(double);

        void writeString(std::string);
        void writeString(slice s);
        void writeData(slice s);

        encoder writeArray(uint32_t count, bool wide =false)
                    {return encoder(*this, kArray, count, wide);}
        encoder writeDict(uint32_t count, bool wide =false)
                    {return encoder(*this, kDict, count, wide);}

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
        encoder(encoder *parent, size_t offset, size_t keyOffset, size_t count, uint8_t width);
        void writeValue(internal::tags, uint8_t *buf, size_t size, bool canInline =true);
        bool writePointerTo(uint64_t dstOffset);
        bool makePointer(uint64_t toOffset, uint8_t buf[]);

        void writeSpecial(uint8_t special);
        void writeInt(uint64_t i, bool isShort, bool isUnsigned);
        void writeData(internal::tags, slice s);
        void writeArrayOrDict(internal::tags, uint32_t count, bool wide, encoder *childEncoder);

        encoder(); // forbidden
        encoder(const encoder&); // forbidden

        typedef std::unordered_map<std::string, uint64_t> stringTable;

        encoder *_parent;       // Encoder that created this one
        size_t _offset;         // Offset in _out to write next inline value
        size_t _keyOffset;      // Offset in _out to write next dictionary key
        size_t _count;          // Count of collection I'm adding to
        Writer& _out;           // Where output is written to
        stringTable *_strings;  // Maps strings to offsets where they appear as values
        uint8_t _width;         // Byte width of array/dict values (2 or 4)
        bool _writingKey    :1; // True if value being written is a key
        bool _blockedOnKey  :1; // True if writes should be refused
    };

}

#endif /* Fleece_Encoder_hh */
