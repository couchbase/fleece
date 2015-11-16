//
//  Value.hh
//  Fleece
//
//  Created by Jens Alfke on 1/25/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#ifndef Fleece_Value_h
#define Fleece_Value_h
#include "Internal.hh"
#include "slice.hh"
#include <ctime>
#include <stdint.h>
#include <unordered_map>
#include <vector>


namespace fleece {

    class array;
    class dict;
    class dictIterator;
        

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

        template<typename T> T asFloatOfType() const;

        float asFloat() const      {return asFloatOfType<float>();}
        double asDouble() const    {return asFloatOfType<double>();}

        bool isInteger() const     {return tag() <= internal::kIntTag;}
        bool isUnsigned() const    {return tag() == internal::kIntTag && (_byte[0] & 0x08) != 0;}
        bool isDouble() const      {return tag() == internal::kFloatTag && tinyValue() == 8;}

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
        internal::tags tag() const             {return (internal::tags)(_byte[0] >> 4);}

        const value* deref() const;

        unsigned tinyValue() const   {return _byte[0] & 0x0F;}
        uint16_t shortValue() const  {return (((uint16_t)_byte[0] << 8) | _byte[1]) & 0x0FFF;}

        uint32_t arrayCount() const;
        const value* arrayFirstAndCount(uint32_t *pCount) const;

        static bool validate(const void* start, slice&);

        uint8_t _byte[2];

        friend class encoder;
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
            const class value* value() const        {return _value;}
            operator const class value* const ()    {return _value;}
            const class value* operator-> ()        {return _value;}

            explicit operator bool() const          {return _count > 0;}
            iterator& operator++();

        private:
            const class value *_p, *_value;
            uint32_t _count;
        };
    };


    /** A value that's a dictionary/map */
    class dict : public value {
    public:
        uint32_t count() const                      {return arrayCount();}

        /** Looks up the value for a key. */
        const value* get(slice key) const;

        class iterator {
        public:
            iterator(const dict*);
            const value* key() const        {return _key;}
            const value* value() const      {return _value;}

            explicit operator bool() const  {return _count > 0;}
            iterator& operator++();

        private:
            const class value *_pKey, *_pValue, *_key, *_value;
            uint32_t _count;
        };

        static uint16_t hashCode(slice);

    private:
        friend class value;
    };

}

#endif
