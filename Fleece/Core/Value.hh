//
// Value.hh
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
#include "Internal.hh"
#include "FleeceException.hh"
#include "fleece/slice.hh"
#include "Endian.hh"
#include <stdint.h>
#include <map>
#ifdef __OBJC__
#import <Foundation/NSMapTable.h>
#endif

namespace fleece {
    class Writer;
}

namespace fleece { namespace impl {
    class Array;
    class Dict;
    class SharedKeys;


    /* Types of values -- same as JSON types, plus binary data */
    enum valueType : uint8_t {
        kNull = 0,
        kBoolean,
        kNumber,
        kString,
        kData,
        kArray,
        kDict
    };


    class Null {
    };

    constexpr Null nullValue;


    /** Option flags for copying values. */
    enum CopyFlags {
        kDefaultCopy        = 0,
        kDeepCopy           = 1,
        kCopyImmutables     = 2,
    };


    /* An encoded data value */
    class Value {
    public:

        /** Returns a pointer to the root value in the encoded data.
            Validates the data first; if it's invalid, returns nullptr.
            Does NOT copy or take ownership of the data; the caller is responsible for keeping it
            intact. Any changes to the data will invalidate any FLValues obtained from it. */
        static const Value* fromData(slice) noexcept;

        /** Returns a pointer to the root value in the encoded data, without validating.
            This is a lot faster, but "undefined behavior" occurs if the data is corrupt... */
        static const Value* fromTrustedData(slice s) noexcept;

        /** The overall type of a value (JSON types plus Data) */
        valueType type() const noexcept FLPURE;

        /** Compares two Values for equality. */
        bool isEqual(const Value*) const FLPURE;

        //////// Scalar types:

        /** Boolean value/conversion. Any value is considered true except false, null, 0. */
        bool asBool() const noexcept FLPURE;

        /** Integer value/conversion. Float values will be rounded. A true value returns 1.
            Other non-numeric values return 0. */
        int64_t asInt() const noexcept FLPURE;

        /** Integer conversion, expressed as an unsigned type. Use this instead of asInt if
            isUnsigned is true, otherwise large 64-bit numbers may look negative. */
        uint64_t asUnsigned() const noexcept FLPURE             {return (uint64_t)asInt();}

        /** 32-bit float value/conversion. Non-numeric values return 0, as with asInt. */
        float asFloat() const noexcept FLPURE      {return asFloatOfType<float>();}

        /** 64-bit float value/conversion. Non-numeric values return 0, as with asInt. */
        double asDouble() const noexcept FLPURE    {return asFloatOfType<double>();}

        /** Is this value an integer? */
        bool isInteger() const noexcept FLPURE     {return tag() <= internal::kIntTag;}

        /** Is this value an unsigned integer? (This does _not_ mean it's positive; it means
            that you should treat it as possibly overflowing an int64_t.) */
        bool isUnsigned() const noexcept FLPURE    {return tag() == internal::kIntTag && (_byte[0] & 0x08) != 0;}

        /** Is this a 64-bit floating-point value? */
        bool isDouble() const noexcept FLPURE      {return tag() == internal::kFloatTag && (_byte[0] & 0x8);}

        /** "undefined" is a special subtype of kNull */
        bool isUndefined() const noexcept FLPURE   {return _byte[0] == ((internal::kSpecialTag << 4) |
                                                                internal::kSpecialValueUndefined);}

        //////// Non-scalars:

        /** Returns the exact contents of a string. Other types return a null slice. */
        slice asString() const noexcept FLPURE;

        /** Returns the exact contents of a binary data value. Other types return a null slice. */
        slice asData() const noexcept FLPURE;

        typedef int64_t FLTimestamp;
        #define FLTimestampNone INT64_MIN

        /** Converts a value to a timestamp, in milliseconds since Unix epoch, or INT64_MIN on failure.
             - A string is parsed as ISO-8601 (standard JSON date format).
             - A number is interpreted as a timestamp and returned as-is. */
        FLTimestamp asTimestamp() const noexcept FLPURE;

        /** If this value is an array, returns it cast to 'const Array*', else returns nullptr. */
        const Array* asArray() const noexcept FLPURE;

        /** If this value is a dictionary, returns it cast to 'const Dict*', else returns nullptr. */
        const Dict* asDict() const noexcept FLPURE;

        static const Array* asArray(const Value *v) FLPURE     {return v ?v->asArray() : nullptr;}
        static const Dict*  asDict(const Value *v) FLPURE      {return v ?v->asDict()  : nullptr;}

        /** Converts any _non-collection_ type to string form. */
        alloc_slice toString() const;

        /** Returns true if this value is a mutable array or dict. */
        bool isMutable() const FLPURE              {return ((size_t)this & 1) != 0;}

        /** Looks up the SharedKeys from the enclosing Doc (if any.) */
        SharedKeys* sharedKeys() const noexcept FLPURE;


        //////// Conversion:

        /** Writes a JSON representation to a Writer.
            If you call it as toJSON<5>(...), writes JSON5, which leaves most keys unquoted. */
        template <int VER =1>
        void toJSON(Writer&) const;

        /** Returns a JSON representation.
            If you call it as toJSON<5>(...), writes JSON5, which leaves most keys unquoted. */
        template <int VER =1>
        alloc_slice toJSON(bool canonical =false) const;

        /** Returns a JSON string representation of a Value. */
        std::string toJSONString() const;

        /** Writes a full dump of the values in the data, including offsets and hex. */
        static bool dump(slice data, std::ostream&);

        /** Returns a full dump of the values in the data, including offsets and hex. */
        static std::string dump(slice data);

        void dump(std::ostream&) const;

        /** A static 'true' Value, as a convenience. */
        static const Value* const kTrueValue;

        /** A static 'false' Value, as a convenience. */
        static const Value* const kFalseValue;

        /** A static 'null' Value, as a convenience.
            (This is not a null pointer, rather a pointer to a Value whose type is kNull.) */
        static const Value* const kNullValue;

        /** A static 'undefined' Value, as a convenience. */
        static const Value* const kUndefinedValue;


#ifdef __OBJC__
        //////// Convenience methods for Objective-C (Cocoa):

        /** Converts a Fleece value to an Objective-C object.
            Can optionally use a pre-existing shared-string table.
            New strings will be added to the table. The table can be used for multiple calls
            and will reduce the number of NSString objects created by the decoder.
            (Not noexcept, but can only throw Objective-C exceptions.) */
        id toNSObject(NSMapTable *sharedStrings =nil) const;

        /** Creates a new shared-string table for use with toNSObject. */
        static NSMapTable* createSharedStringsTable() noexcept;
#endif

        void _retain() const;
        void _release() const;

    protected:
        constexpr Value(internal::tags tag, int tiny, int byte1 = 0)
        :_byte {(uint8_t)((tag<<4) | tiny),
                (uint8_t)byte1}
        { }

        static const Value* findRoot(slice) noexcept FLPURE;
        bool validate(const void* dataStart, const void *dataEnd) const noexcept FLPURE;

        internal::tags tag() const noexcept FLPURE   {return (internal::tags)(_byte[0] >> 4);}
        unsigned tinyValue() const noexcept FLPURE   {return _byte[0] & 0x0F;}

        // numbers:
        uint16_t shortValue() const noexcept FLPURE  {return (((uint16_t)_byte[0] << 8) | _byte[1]) & 0x0FFF;}
        template<typename T> T asFloatOfType() const noexcept FLPURE;

        // strings:
        slice getStringBytes() const noexcept FLPURE;

        // arrays/dicts:
        bool isWideArray() const noexcept FLPURE     {return (_byte[0] & 0x08) != 0;}
        uint32_t countValue() const noexcept FLPURE  {return (((uint32_t)_byte[0] << 8) | _byte[1]) & 0x07FF;}
        bool countIsZero() const noexcept FLPURE     {return _byte[1] == 0 && (_byte[0] & 0x7) == 0;}

        // pointers:

        bool isPointer() const noexcept FLPURE             {return (_byte[0] & 0x80) != 0;}
        const internal::Pointer* _asPointer() const FLPURE {return (const internal::Pointer*)this;}

        const Value* deref(bool wide) const FLPURE;

        template <bool WIDE>
        const Value* deref() const FLPURE;

        const Value* next(bool wide) const noexcept FLPURE
                                {return offsetby(this, wide ? internal::kWide : internal::kNarrow);}
        template <bool WIDE>
        FLPURE const Value* next() const noexcept         {return next(WIDE);}

        // dump:
        size_t dataSize() const noexcept FLPURE;
        typedef std::map<size_t, const Value*> mapByAddress;
        void mapAddresses(mapByAddress&) const;
        static void writeByAddress(const mapByAddress &byAddress, slice data, std::ostream &out);
        size_t dump(std::ostream &out, bool wide, int indent, const void *base) const;
        void writeDumpBrief(std::ostream &out, const void *base, bool wide =false) const;

        //////// Here's the data:

        uint8_t _byte[internal::kWide];

        friend class internal::Pointer;
        friend class ValueSlot;
        friend class internal::HeapCollection;
        friend class internal::HeapValue;
        friend class Array;
        friend class Dict;
        friend class Encoder;
        friend class ValueTests;
        friend class EncoderTests;
        template <bool WIDE> friend struct dictImpl;
    };


    // Some glue needed to make RefCounted<Value> work
    static inline void release(const Value *val) noexcept {
        if (val) val->_release();
    }
    static inline void copyRef(void *dstPtr, const Value *src) noexcept {
        auto old = *(const Value**)dstPtr;
        if (src) src->_retain();
        *(const Value**)dstPtr = src;
        if (old) old->_release();
    }

} }
