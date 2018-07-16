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
#include "slice.hh"
#include "Endian.hh"
#include <stdint.h>
#include <map>
#ifdef __OBJC__
#import <Foundation/NSMapTable.h>
#endif


namespace fleece {

    class Array;
    class Dict;
    class Writer;
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
        valueType type() const noexcept;

        //////// Scalar types:

        /** Boolean value/conversion. Any value is considered true except false, null, 0. */
        bool asBool() const noexcept;

        /** Integer value/conversion. Float values will be rounded. A true value returns 1.
            Other non-numeric values return 0. */
        int64_t asInt() const noexcept;

        /** Integer conversion, expressed as an unsigned type. Use this instead of asInt if
            isUnsigned is true, otherwise large 64-bit numbers may look negative. */
        uint64_t asUnsigned() const noexcept             {return (uint64_t)asInt();}

        /** 32-bit float value/conversion. Non-numeric values return 0, as with asInt. */
        float asFloat() const noexcept      {return asFloatOfType<float>();}

        /** 64-bit float value/conversion. Non-numeric values return 0, as with asInt. */
        double asDouble() const noexcept    {return asFloatOfType<double>();}

        /** Is this value an integer? */
        bool isInteger() const noexcept     {return tag() <= internal::kIntTag;}

        /** Is this value an unsigned integer? (This does _not_ mean it's positive; it means
            that you should treat it as possibly overflowing an int64_t.) */
        bool isUnsigned() const noexcept    {return tag() == internal::kIntTag && (_byte[0] & 0x08) != 0;}

        /** Is this a 64-bit floating-point value? */
        bool isDouble() const noexcept      {return tag() == internal::kFloatTag && (_byte[0] & 0x8);}

        /** "undefined" is a special subtype of kNull */
        bool isUndefined() const noexcept   {return tag() == internal::kSpecialTag
                                             && tinyValue() == internal::kSpecialValueUndefined;}

        //////// Non-scalars:

        /** Returns the exact contents of a string. Other types return a null slice. */
        slice asString() const noexcept;

        /** Returns the exact contents of a binary data value. Other types return a null slice. */
        slice asData() const noexcept;

        /** If this value is an array, returns it cast to 'const Array*', else returns nullptr. */
        const Array* asArray() const noexcept;

        /** If this value is a dictionary, returns it cast to 'const Dict*', else returns nullptr. */
        const Dict* asDict() const noexcept;

        static const Array* asArray(const Value *v)     {return v ?v->asArray() : nullptr;}
        static const Dict*  asDict(const Value *v)      {return v ?v->asDict()  : nullptr;}

        /** Converts any _non-collection_ type to string form. */
        alloc_slice toString() const;

        /** Returns true if this value is a mutable array or dict. */
        bool isMutable() const              {return ((size_t)this & 1) != 0;}

        //////// Conversion:

        /** Writes a JSON representation to a Writer.
            If you call it as toJSON<5>(...), writes JSON5, which leaves most keys unquoted. */
        template <int VER =1>
        void toJSON(Writer&, const SharedKeys* =nullptr) const;

        /** Returns a JSON representation.
            If you call it as toJSON<5>(...), writes JSON5, which leaves most keys unquoted. */
        template <int VER =1>
        alloc_slice toJSON(const SharedKeys* =nullptr, bool canonical =false) const;

        /** Returns a JSON string representation of a Value. */
        std::string toJSONString() const;

        /** Writes a full dump of the values in the data, including offsets and hex. */
        static bool dump(slice data, std::ostream&);

        /** Returns a full dump of the values in the data, including offsets and hex. */
        static std::string dump(slice data);

        void dump(std::ostream&) const;

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
        id toNSObject(NSMapTable *sharedStrings =nil, const SharedKeys* = nullptr) const;

        /** Creates a new shared-string table for use with toNSObject. */
        static NSMapTable* createSharedStringsTable() noexcept;
#endif

        void _retain();
        void _release();

    protected:
        constexpr Value(internal::tags tag, int tiny, int byte1 = 0)
        :_byte {(uint8_t)((tag<<4) | tiny),
                (uint8_t)byte1,
                0, 0}
        { }

        static const Value kNullInstance, kUndefinedInstance;

        static const Value* findRoot(slice) noexcept;
        bool validate(const void* dataStart, const void *dataEnd) const noexcept;

        internal::tags tag() const noexcept   {return (internal::tags)(_byte[0] >> 4);}
        unsigned tinyValue() const noexcept   {return _byte[0] & 0x0F;}

        // numbers:
        uint16_t shortValue() const noexcept  {return (((uint16_t)_byte[0] << 8) | _byte[1]) & 0x0FFF;}
        template<typename T> T asFloatOfType() const noexcept;

        // strings:
        slice getStringBytes() const noexcept;

        // arrays/dicts:
        bool isWideArray() const noexcept     {return (_byte[0] & 0x08) != 0;}
        uint32_t countValue() const noexcept  {return (((uint32_t)_byte[0] << 8) | _byte[1]) & 0x07FF;}
        bool countIsZero() const noexcept     {return _byte[1] == 0 && (_byte[0] & 0x7) == 0;}

        // pointers:

        bool isPointer() const noexcept             {return (_byte[0] & 0x80) != 0;}
        const internal::Pointer* _asPointer() const {return (const internal::Pointer*)this;}

        const Value* deref(bool wide) const;

        template <bool WIDE>
        const Value* deref() const;

        const Value* next(bool wide) const noexcept
                                {return offsetby(this, wide ? internal::kWide : internal::kNarrow);}
        template <bool WIDE>
        const Value* next() const noexcept       {return next(WIDE);}

        // dump:
        size_t dataSize() const noexcept;
        typedef std::map<size_t, const Value*> mapByAddress;
        void mapAddresses(mapByAddress&) const;
        static void writeByAddress(const mapByAddress &byAddress, slice data, std::ostream &out);
        size_t dump(std::ostream &out, bool wide, int indent, const void *base) const;
        void writeDumpBrief(std::ostream &out, const void *base, bool wide =false) const;

        //////// Here's the data:

        uint8_t _byte[internal::kWide];

        friend class internal::Pointer;
        friend class internal::ValueSlot;
        friend class internal::HeapCollection;
        friend class internal::HeapValue;
        friend class Array;
        friend class Dict;
        friend class Encoder;
        friend class ValueTests;
        friend class EncoderTests;
        template <bool WIDE> friend struct dictImpl;
    };

}
