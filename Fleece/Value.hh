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
#include <stdint.h>


namespace fleece {

    class array;
    class dict;


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
        static const value* fromTrustedData(slice s)    {return (const value*)s.buf;}

        valueType type() const;

        bool asBool() const;
        int64_t asInt() const;
        uint64_t asUnsigned() const             {return (uint64_t)asInt();}

        float asFloat() const      {return asFloatOfType<float>();}
        double asDouble() const    {return asFloatOfType<double>();}

        bool isInteger() const     {return tag() <= internal::kIntTag;}
        bool isUnsigned() const    {return tag() == internal::kIntTag && (_byte[0] & 0x08) != 0;}
        bool isDouble() const      {return tag() == internal::kFloatTag && (_byte[0] & 0x8);}

        /** Returns the exact contents of a string or data. */
        slice asString() const;

        const array* asArray() const;
        const dict* asDict() const;

        /** Converts any non-collection type to string form. */
        std::string toString() const;

        /** Writes a JSON representation to an ostream. */
        void writeJSON(std::ostream&) const;
        /** Returns a JSON representation. */
        std::string toJSON() const;

#ifdef __OBJC__
        id asNSObject() const;
        id asNSObject(NSMapTable *sharedStrings) const;
        static NSMapTable* createSharedStringsTable();
#endif

    protected:
        bool isPointer() const       {return (_byte[0] >= (internal::kPointerTagFirst << 4));}
        template <bool WIDE>
            static const value* derefPointer(const value *v);
        static const value* deref(const value *v, bool wide);
        template <bool WIDE>
            static const value* deref(const value *v);

        struct arrayInfo {
            const value* first;
            uint32_t count;
        };
        unsigned isWideArray() const {return (_byte[0] & 0x08) != 0;}
        arrayInfo getArrayInfo() const;
        uint32_t arrayCount() const  {return getArrayInfo().count;}

    private:
        internal::tags tag() const   {return (internal::tags)(_byte[0] >> 4);}
        unsigned tinyValue() const   {return _byte[0] & 0x0F;}
        uint16_t shortValue() const  {return (((uint16_t)_byte[0] << 8) | _byte[1]) & 0x0FFF;}
        template<typename T> T asFloatOfType() const;

        static bool validate(const void* start, slice&);

        uint8_t _byte[2];

        friend class array;
        friend class dict;
    };


    /** A value that's an array. */
    class array : public value {
    public:
        uint32_t count() const                      {return arrayCount();}
        const value* get(uint32_t index) const;

        class iterator {
        public:
            iterator(const array* a);
            uint32_t count() const                  {return _a.count;}
            const class value* value() const        {return _value;}
            operator const class value* const ()    {return _value;}
            const class value* operator-> ()        {return _value;}
            const class value* operator[] (unsigned);

            explicit operator bool() const          {return _a.count > 0;}
            iterator& operator++();

        private:
            arrayInfo _a;
            const class value *_value;
            bool _wide;
        };
    };


    /** A value that's a dictionary/map */
    class dict : public value {
    public:
        uint32_t count() const                      {return arrayCount();}

        /** Looks up the value for a key. */
        const value* get(slice key) const;

        const value* get_sorted(slice keyToFind) const;

        class iterator {
        public:
            iterator(const dict*);
            uint32_t count() const                  {return _a.count;}
            const value* key() const                {return _key;}
            const value* value() const              {return _value;}

            explicit operator bool() const          {return _a.count > 0;}
            iterator& operator++();

        private:
            arrayInfo _a;
            const class value *_pValue, *_key, *_value;
            bool _wide;
        };

    private:
        template <bool WIDE>
            static int keyCmp(const void* keyToFindP, const void* keyP);
        template <bool WIDE>
            const value* get_sorted(slice keyToFind) const;

        friend class value;
    };

}

#endif
