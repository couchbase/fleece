//
//  Value.hh
//  Fleece
//
//  Created by Jens Alfke on 1/25/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#pragma once
#include "Internal.hh"
#include "FleeceException.hh"
#include "slice.hh"
#include "Endian.hh"
#include "varint.hh"
#include <stdint.h>
#include <map>
#ifdef __OBJC__
#import <Foundation/NSMapTable.h>
#endif


namespace fleece {

    class Array;
    class Dict;
    class Writer;


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


    /* An encoded data value */
    class Value {
    public:

        /** Returns a pointer to the root value in the encoded data.
            Validates the data first; if it's invalid, returns NULL.
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

        //////// Non-scalars:

        /** Returns the exact contents of a string or data. Other types return a null slice. */
        slice asString() const noexcept;

        /** If this value is an array, returns it cast to 'const Array*', else returns NULL. */
        const Array* asArray() const noexcept;

        /** If this value is a dictionary, returns it cast to 'const Dict*', else returns NULL. */
        const Dict* asDict() const noexcept;

        /** Converts any _non-collection_ type to string form. */
        alloc_slice toString() const;

        //////// Conversion:

        /** Writes a JSON representation to a Writer. */
        void toJSON(Writer&) const;
        /** Returns a JSON representation. */
        alloc_slice toJSON() const;

        /** Writes a full dump of the values in the data, including offsets and hex. */
        static bool dump(slice data, std::ostream&);

        /** Returns a full dump of the values in the data, including offsets and hex. */
        static std::string dump(slice data);

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

    protected:
        bool isPointer() const noexcept       {return (_byte[0] >= (internal::kPointerTagFirst << 4));}

        template <bool WIDE>
        uint32_t pointerValue() const noexcept {
            if (WIDE)
                return (_dec32(*(uint32_t*)_byte) & ~0x80000000) << 1;
            else
                return (_dec16(*(uint16_t*)_byte) & ~0x8000) << 1;
        }

        void shrinkPointer() noexcept {
            _byte[0] = _byte[2] | 0x80;
            _byte[1] = _byte[3];
        }

        template <bool WIDE>
        static const Value* derefPointer(const Value *v) {
            return offsetby(v, -(ptrdiff_t)v->pointerValue<WIDE>());
        }
        static const Value* derefPointer(const Value *v, bool wide) {
            return wide ? derefPointer<true>(v) : derefPointer<false>(v);
        }
        static const Value* deref(const Value *v, bool wide);

        template <bool WIDE>
        static const Value* deref(const Value *v);

        const Value* next(bool wide) const noexcept
                                {return offsetby(this, wide ? internal::kWide : internal::kNarrow);}
        template <bool WIDE>
        const Value* next() const noexcept       {return next(WIDE);}

        bool isWideArray() const noexcept {return (_byte[0] & 0x08) != 0;}

    private:
        Value(internal::tags tag, int tiny, int byte1 = 0) {
            _byte[0] = (uint8_t)((tag<<4) | tiny);
            _byte[1] = (uint8_t)byte1;
            _byte[2] = _byte[3] = 0;
        }

        // pointer:
        Value(size_t offset, int width) {
            offset >>= 1;
            if (width < internal::kWide) {
                if (offset >= 0x8000)
                    throw FleeceException(InternalError, "offset too large");
                int16_t n = (uint16_t)_enc16(offset | 0x8000); // big-endian, high bit set
                memcpy(_byte, &n, sizeof(n));
            } else {
                if (offset >= 0x80000000)
                    throw FleeceException(OutOfRange, "data too large");
                uint32_t n = (uint32_t)_enc32(offset | 0x80000000);
                memcpy(_byte, &n, sizeof(n));
            }
        }

        internal::tags tag() const noexcept   {return (internal::tags)(_byte[0] >> 4);}
        unsigned tinyValue() const noexcept   {return _byte[0] & 0x0F;}
        uint16_t shortValue() const noexcept  {return (((uint16_t)_byte[0] << 8) | _byte[1]) & 0x0FFF;}
        template<typename T> T asFloatOfType() const noexcept;

        slice getStringBytes() const noexcept;

        // dump:
        size_t dataSize() const noexcept;
        typedef std::map<size_t, const Value*> mapByAddress;
        void mapAddresses(mapByAddress&) const;
        void dump(std::ostream &out, bool wide, int indent, const void *base) const;
        void writeDumpBrief(std::ostream &out, const void *base, bool wide =false) const;

        static const Value* fastValidate(slice) noexcept;
        bool validate(const void* dataStart, const void *dataEnd, bool wide) const noexcept;

        //////// Here's the data:

        uint8_t _byte[internal::kWide];

        friend class Array;
        friend class Dict;
        friend class Encoder;
        friend class ValueTests;
        friend class EncoderTests;
        friend class Arr;
        template <bool WIDE> friend struct dictImpl;
    };

}
