//
//  Value.hh
//  Fleece
//
//  Created by Jens Alfke on 1/25/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#ifndef Fleece_Value_hh
#define Fleece_Value_hh
#include "Internal.hh"
#include "slice.hh"
#include "Endian.hh"
#include <stdint.h>
#include <map>
#ifdef __OBJC__
#import <Foundation/NSMapTable.h>
#endif


namespace fleece {

    class array;
    class dict;
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
    class value {
    public:

        /** Returns a pointer to the root value in the encoded data.
            Validates the data first; if it's invalid, returns NULL. */
        static const value* fromData(slice);

        /** Returns a pointer to the root value in the encoded data, without validating.
            This is a lot faster, but "undefined behavior" occurs if the data is corrupt... */
        static const value* fromTrustedData(slice s);

        /** The overall type of a value (JSON types plus Data) */
        valueType type() const;

        //////// Scalar types:

        /** Boolean value/conversion. Any value is considered true except false, null, 0. */
        bool asBool() const;

        /** Integer value/conversion. Float values will be rounded. A true value returns 1.
            Other non-numeric values return 0. */
        int64_t asInt() const;

        /** Integer conversion, expressed as an unsigned type. Use this instead of asInt if
            isUnsigned is true, otherwise large 64-bit numbers may look negative. */
        uint64_t asUnsigned() const             {return (uint64_t)asInt();}

        /** 32-bit float value/conversion. Non-numeric values return 0, as with asInt. */
        float asFloat() const      {return asFloatOfType<float>();}

        /** 64-bit float value/conversion. Non-numeric values return 0, as with asInt. */
        double asDouble() const    {return asFloatOfType<double>();}

        /** Is this value an integer? */
        bool isInteger() const     {return tag() <= internal::kIntTag;}

        /** Is this value an unsigned integer? (This does _not_ mean it's positive; it means
            that you should treat it as possibly overflowing an int64_t.) */
        bool isUnsigned() const    {return tag() == internal::kIntTag && (_byte[0] & 0x08) != 0;}

        /** Is this a 64-bit floating-point value? */
        bool isDouble() const      {return tag() == internal::kFloatTag && (_byte[0] & 0x8);}

        //////// Non-scalars:

        /** Returns the exact contents of a string or data. Other types return a null slice. */
        slice asString() const;

        /** If this value is an array, returns it cast to 'const array*', else returns NULL. */
        const array* asArray() const;

        /** If this value is an array, returns it cast to 'const array*', else returns NULL. */
        const dict* asDict() const;

        /** Converts any _non-collection_ type to string form. */
        std::string toString() const;

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
            and will reduce the number of NSString objects created by the decoder. */
        id toNSObject(NSMapTable *sharedStrings =nil) const;

        /** Creates a new shared-string table for use with toNSObject. */
        static NSMapTable* createSharedStringsTable();
#endif

    protected:
        bool isPointer() const       {return (_byte[0] >= (internal::kPointerTagFirst << 4));}

        template <bool WIDE>
        uint32_t pointerValue() const {
            if (WIDE)
                return (_dec32(*(uint32_t*)_byte) & ~0x80000000) << 1;
            else
                return (_dec16(*(uint16_t*)_byte) & ~0x8000) << 1;
        }

        void shrinkPointer() {
            _byte[0] = _byte[2] | 0x80;
            _byte[1] = _byte[3];
        }

        template <bool WIDE>
        static const value* derefPointer(const value *v) {
            return offsetby(v, -(ptrdiff_t)v->pointerValue<WIDE>());
        }
        static const value* derefPointer(const value *v, bool wide) {
            return wide ? derefPointer<true>(v) : derefPointer<false>(v);
        }
        static const value* deref(const value *v, bool wide);

        template <bool WIDE>
        static const value* deref(const value *v) {
            if (v->isPointer()) {
                v = derefPointer<WIDE>(v);
                while (v->isPointer())
                    v = derefPointer<true>(v);      // subsequent pointers must be wide
            }
            return v;
        }



        const value* next(bool wide) const
                                {return offsetby(this, wide ? internal::kWide : internal::kNarrow);}
        template <bool WIDE>
        const value* next() const       {return next(WIDE);}

        struct arrayInfo {
            const value* first;
            uint32_t count;
            bool wide;

            arrayInfo(const value*);
            const value* second() const      {return first->next(wide);}
            bool next();
            const value* firstValue() const  {return count ? deref(first, wide) : NULL;}
            const value* operator[] (unsigned index) const;
            size_t indexOf(const value *v) const;
        };

        bool isWideArray() const {return (_byte[0] & 0x08) != 0;}
        uint32_t arrayCount() const  {return arrayInfo(this).count;}

    private:
        value(internal::tags tag, int tiny, int byte1 = 0) {
            _byte[0] = (uint8_t)((tag<<4) | tiny);
            _byte[1] = (uint8_t)byte1;
            _byte[2] = _byte[3] = 0;
        }

        // pointer:
        value(size_t offset, int width) {
            offset >>= 1;
            if (width < internal::kWide) {
                if (offset >= 0x8000)
                    throw "offset too large";
                int16_t n = (uint16_t)_enc16(offset | 0x8000); // big-endian, high bit set
                memcpy(_byte, &n, sizeof(n));
            } else {
                if (offset >= 0x80000000)
                    throw "offset too large";
                uint32_t n = (uint32_t)_enc32(offset | 0x80000000);
                memcpy(_byte, &n, sizeof(n));
            }
        }

        internal::tags tag() const   {return (internal::tags)(_byte[0] >> 4);}
        unsigned tinyValue() const   {return _byte[0] & 0x0F;}
        uint16_t shortValue() const  {return (((uint16_t)_byte[0] << 8) | _byte[1]) & 0x0FFF;}
        template<typename T> T asFloatOfType() const;

        // dump:
        size_t dataSize() const;
        typedef std::map<size_t, const value*> mapByAddress;
        void mapAddresses(mapByAddress&) const;
        void dump(std::ostream &out, bool wide, int indent, const void *base) const;
        void writeDumpBrief(std::ostream &out, const void *base, bool wide =false) const;

        static const value* fastValidate(slice);
        bool validate(const void* dataStart, const void *dataEnd, bool wide) const;

        //////// Here's the data:

        uint8_t _byte[internal::kWide];

        friend class array;
        friend class dict;
        friend class Encoder;
        friend class ValueTests;
        friend class EncoderTests;
    };

}

#endif
