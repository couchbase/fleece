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
#include "StringTable.hh"
#include <ctime>
#include <iostream>
#include <unordered_map>


namespace fleece {

    /** Generates Fleece-encoded data. */
    class Encoder {
    public:
        /** Constructs an encoder that will write to the given Writer. */
        Encoder(Writer&);

        Encoder(Encoder&&);  // Move constructor

        /** Returns the Writer that the encoded data is written to. */
        Writer& writer() const          {return _out;}

        /** Sets the uniqueStrings property. If true (the default), the encoder tries to write
            each unique string only once. This saves space but makes the encoder slightly slower. */
        void uniqueStrings(bool b)      {_uniqueStrings = b;}

        /** Sets the sortKeys property. If true (the default), dictionary keys will be written in
            sorted order. This makes dict::get faster but makes the encoder slightly slower. */
        void sortKeys(bool b)           {_sortKeys = b;}

        /** Ends encoding, writing the last of the data to the Writer. */
        void end();

        /** Resets the encoder so it can be used again. This creates a new empty Writer,
            which can be accessed via the writer() method. */
        void reset();

        void writeNull();
        void writeBool (bool);

        void writeInt(int64_t i);
        void writeUInt(uint64_t i);
        void writeFloat(float);
        void writeDouble(double);

        void writeString(std::string);
        void writeString(slice s)           {(void)_writeString(s, false);}
        void writeData(slice s);

        /** Begins creating an array. Until endArray is called, values written to the encoder are
            added to this array.
            @param reserve  If nonzero, space is preallocated for this many values. This has no
                            effect on the output but can speed up encoding slightly. */
        void beginArray(size_t reserve =0);

        /** Ends creating an array. The array is written to the output and added as a value to
            the next outermost collection (or made the root if there is no collection active.) */
        void endArray();

        /** Begins creating a dictionary. Until endDict is called, values written to the encoder
            are added to this dictionary.
            While creating a dictionary, writeKey must be called before every value.
            @param reserve  If nonzero, space is preallocated for this many values. This has no
                            effect on the output but can speed up encoding slightly. */
        void beginDictionary(size_t reserve =0);

        /** Writes a key to the current dictionary. This must be called before adding a value. */
        void writeKey(std::string);
        /** Writes a key to the current dictionary. This must be called before adding a value. */
        void writeKey(slice);

        /** Ends creating a dictionary. The dict is written to the output and added as a value to
            the next outermost collection (or made the root if there is no collection active.) */
        void endDictionary();

        void writeKeyTable();

        // Note: overriding <<(bool) would be dangerous due to implicit conversion
        Encoder& operator<< (int64_t i)         {writeInt(i); return *this;}
        Encoder& operator<< (double d)          {writeDouble(d); return *this;}
        Encoder& operator<< (float f)           {writeFloat(f); return *this;}
        Encoder& operator<< (std::string str)   {writeString(str); return *this;}
        Encoder& operator<< (slice s)           {writeString(s); return *this;} // string not data!

#ifdef __OBJC__
        /** Writes an Objective-C object. Supported classes are the ones allowed by
            NSJSONSerialization, as well as NSData. */
        void write(id);
#endif

    private:
        // Stores the pending values to be written to an in-progress array/dict
        class valueArray : public std::vector<value> {
        public:
            valueArray()                    { }
            void reset(internal::tags t)    {tag = t; wide = false; keys.clear();}
            internal::tags tag;
            bool wide;
            std::vector<slice> keys;
        };

        void addItem(value v);
        void writeValue(internal::tags, uint8_t buf[], size_t size, bool canInline =true);
        void writePointer(size_t pos);
        void writeSpecial(uint8_t special);
        void writeInt(uint64_t i, bool isShort, bool isUnsigned);
        void _writeFloat(float);
        slice writeData(internal::tags, slice s);
        slice _writeString(slice, bool asKey);
        size_t nextWritePos();
        void sortDict(valueArray &items);
        void checkPointerWidths(valueArray *items);
        void fixPointers(valueArray *items);
        void endCollection(internal::tags tag);
        void push(internal::tags tag, size_t reserve);

        Encoder(); // forbidden
        Encoder(const Encoder&); // forbidden

        Writer& _out;           // Where output is written to
        valueArray *_items;     // Values of the currently-open array/dict; == &_stack[_stackDepth]
        std::vector<valueArray> _stack; // Stack of open arrays/dicts
        unsigned _stackDepth;   // Current depth of _stack
        StringTable _strings;   // Maps strings to the offsets where they appear as values
        bool _uniqueStrings;    // Should strings be uniqued before writing?
        bool _sortKeys;         // Should dictionary keys be sorted?
        bool _writingKey;       // True if value being written is a key
        bool _blockedOnKey;     // True if writes should be refused

        friend class EncoderTests;
#ifndef NDEBUG
    public:
        unsigned _numNarrow, _numWide, _narrowCount, _wideCount, _numSavedStrings;
#endif
    };

}

#endif /* Fleece_Encoder_hh */
