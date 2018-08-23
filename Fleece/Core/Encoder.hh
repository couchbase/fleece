//
// Encoder.hh
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

#pragma once

#include "Value.hh"
#include "Writer.hh"
#include "StringTable.hh"
#include <array>
#include "function_ref.hh"
#include <vector>


namespace fleece {
    class SharedKeys;


    /** Generates Fleece-encoded data. */
    class Encoder {
    public:
        /** Constructs an encoder. */
        Encoder(size_t reserveOutputSize =256);
        Encoder(FILE* NONNULL);

        /** Sets the uniqueStrings property. If true (the default), the encoder tries to write
            each unique string only once. This saves space but makes the encoder slightly slower. */
        void uniqueStrings(bool b)      {_uniqueStrings = b;}

        /** Sets the base Fleece data that the encoded data will be (logically) appended to.
            Any writeValue() calls whose Value points into the base data will be written as
            pointers.
            @param base  The base Fleece document that's being appended to.
            @param markExternPointers  If true, pointers into the base document (i.e. out of the
                        encoded data) will be marked with the `extern` flag. This allows the use
                        of an ExternResolver, so the new document can be used without having to
                        append it onto the base.
            @param cutoff  If nonzero, this specifies the maximum number of bytes of the base
                            (starting from the end) that should be used. Any base data before
                            the cutoff will not be referenced in the encoder output. */
        void setBase(slice base, bool markExternPointers =false, size_t cutoff =0);

        /** Scans the base document for strings and adds them to the encoder's string table.
            If equivalent strings are written to the encoder they will then be encoded as pointers
            to the existing strings. */
        void reuseBaseStrings();

        bool valueIsInBase(const Value *value NONNULL) const;

        bool isEmpty() const            {return _out.length() == 0 && _stackDepth == 1 && _items->empty();}
        size_t bytesWritten() const     {return _out.length();} // may be an underestimate

        /** Ends encoding, writing the last of the data to the Writer. */
        void end();

        /** Returns the encoded data. This implicitly calls end(). */
        alloc_slice extractOutput();

        /** Resets the encoder so it can be used again. */
        void reset();

        /////// Writing data:

        void writeNull();
        void writeBool (bool);

        void writeInt(int64_t i);
        void writeUInt(uint64_t i);
        void writeFloat(float);
        void writeDouble(double);

        void writeString(const std::string&);
        void writeString(slice s)           {(void)_writeString(s);}

        void writeData(slice s);

        void writeValue(const Value* NONNULL v,
                        const SharedKeys *sk =nullptr)      {writeValue(v, sk, nullptr);}

        using WriteValueFunc = function_ref<bool(const Value *key, const Value *value)>;

        /** Alternative writeValue that invokes a callback before writing any Value.
            If the callback returns false, the value is written as usual, otherwise it's skipped;
            the callback can invoke the Encoder to write a different Value instead if it likes. */
        void writeValue(const Value* NONNULL v,
                        const SharedKeys *sk,
                        WriteValueFunc fn)                  {writeValue(v, sk, &fn);}

#ifdef __OBJC__
        /** Writes an Objective-C object. Supported classes are the ones allowed by
            NSJSONSerialization, as well as NSData. */
        void writeObjC(id);
#endif

        //////// Writing arrays:

        /** Begins creating an array. Until endArray is called, values written to the encoder are
            added to this array.
            @param reserve  If nonzero, space is preallocated for this many values. This has no
                            effect on the output but can speed up encoding slightly. */
        void beginArray(size_t reserve =0);

        /** Ends creating an array. The array is written to the output and added as a Value to
            the next outermost collection (or made the root if there is no collection active.) */
        void endArray();

        //////// Writing dictionaries:

        /** Begins creating a dictionary. Until endDict is called, values written to the encoder
            are added to this dictionary.
            While creating a dictionary, writeKey must be called before every value.
            @param reserve  If nonzero, space is preallocated for this many values. This has no
                            effect on the output but can speed up encoding slightly. */
        void beginDictionary(size_t reserve =0);

        /** Begins creating a dictionary which inherits from an existing dictionary. */
        void beginDictionary(const Dict *parent NONNULL, size_t reserve =0);

        /** Ends creating a dictionary. The dict is written to the output and added as a value to
            the next outermost collection (or made the root if there is no collection active.) */
        void endDictionary();

        /** Writes a key to the current dictionary. This must be called before adding a value. */
        void writeKey(const std::string&);
        /** Writes a key to the current dictionary. This must be called before adding a value. */
        void writeKey(slice);

        /** Writes a numeric key (encoded with SharedKeys) to the current dictionary. */
        void writeKey(int);

        /** Writes a string Value as a key to the current dictionary. */
        void writeKey(const Value* NONNULL);
        void writeKey(const Value* NONNULL, const SharedKeys*);

        /** Associates a SharedKeys object with this Encoder. The writeKey() methods that take
            strings will consult this object to possibly map the key to an integer. */
        void setSharedKeys(SharedKeys *s) {_sharedKeys = s;}

        //////// "<<" convenience operators;

        // Note: overriding <<(bool) would be dangerous due to implicit conversion
        Encoder& operator<< (Null)              {writeNull(); return *this;}
        Encoder& operator<< (long long i)       {writeInt(i); return *this;}
        Encoder& operator<< (unsigned long long i)  {writeUInt(i); return *this;}
        Encoder& operator<< (long i)            {writeInt(i); return *this;}
        Encoder& operator<< (unsigned long i)   {writeUInt(i); return *this;}
        Encoder& operator<< (int i)             {writeInt(i); return *this;}
        Encoder& operator<< (unsigned int i)    {writeUInt(i); return *this;}
        Encoder& operator<< (double d)          {writeDouble(d); return *this;}
        Encoder& operator<< (float f)           {writeFloat(f); return *this;}
        Encoder& operator<< (const std::string &str)   {writeString(str); return *this;}
        Encoder& operator<< (slice s)           {writeString(s); return *this;} // string not data!
        Encoder& operator<< (const Value *v NONNULL)    {writeValue(v); return *this;}


        // For advanced use cases only... be careful!
        void suppressTrailer()                  {_trailer = false;}
        void writeRaw(slice s)                  {_out.write(s);}
        size_t nextWritePos();
        size_t finishItem();
        slice base() const                      {return _base;}
        slice baseUsed() const                  {return _baseMinUsed != 0 ? slice(_baseMinUsed, _base.end()) : nullslice;}

    private:
        // Stores the pending values to be written to an in-progress array/dict
        class valueArray : public std::vector<Value> {
        public:
            valueArray()                    { }
            void reset(internal::tags t)    {tag = t; wide = false; keys.clear();}
            internal::tags tag;
            bool wide;
            std::vector<slice> keys;
        };

        void addItem(Value v);
        void writeRawValue(slice rawValue, bool canInline =true);
        void writeValue(internal::tags, uint8_t buf[], size_t size, bool canInline =true);
        void reuseBaseStrings(const Value* NONNULL);
        void cacheString(slice s, size_t offsetInBase);
        static bool isNarrowValue(const Value *value NONNULL);
        void writePointer(ssize_t pos);
        void writeSpecial(uint8_t special);
        void writeInt(uint64_t i, bool isShort, bool isUnsigned);
        void _writeFloat(float);
        slice writeData(internal::tags, slice s);
        slice _writeString(slice);
        void addingKey();
        void addedKey(slice str);
        void sortDict(valueArray &items);
        void checkPointerWidths(valueArray *items NONNULL, size_t writePos);
        void fixPointers(valueArray *items NONNULL);
        void endCollection(internal::tags tag);
        void push(internal::tags tag, size_t reserve);
        void writeValue(const Value* NONNULL, const SharedKeys* const, const WriteValueFunc*);
        const Value* minUsed(const Value *value);

        Encoder(const Encoder&) = delete;
        Encoder& operator=(const Encoder&) = delete;

        //////// Data members:

        Writer _out;            // Where output is written to
        valueArray *_items;     // Values of the currently-open array/dict; == &_stack[_stackDepth]
        std::vector<valueArray> _stack; // Stack of open arrays/dicts
        unsigned _stackDepth {0};    // Current depth of _stack
        StringTable _strings;        // Maps strings to the offsets where they appear as values
        Writer _stringStorage;       // Backing store for strings in _strings
        bool _uniqueStrings {true};  // Should strings be uniqued before writing?
        SharedKeys *_sharedKeys {nullptr};  // Client-provided key-to-int mapping
        slice _base;                 // Base Fleece data being appended to (if any)
        const void* _baseCutoff {0}; // Lowest addr in _base that I can write a ptr to
        const void* _baseMinUsed {0};// Lowest addr in _base I've written a ptr to
        int _copyingCollection {0};  // Nonzero inside writeValue when writing array/dict
        bool _writingKey    {false}; // True if Value being written is a key
        bool _blockedOnKey  {false}; // True if writes should be refused
        bool _trailer       {true};  // Write standard trailer at end?
        bool _markExternPtrs{false}; // Mark pointers outside encoded data as 'extern'

        friend class EncoderTests;
#ifndef NDEBUG
    public: // Statistics for use in tests
        unsigned _numNarrow {0}, _numWide {0}, _narrowCount {0}, _wideCount {0},
                 _numSavedStrings {0};
#endif
    };

}

