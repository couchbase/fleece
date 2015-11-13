//
//  Value.hh
//  Fleece
//
//  Created by Jens Alfke on 1/25/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#ifndef Fleece_Value_h
#define Fleece_Value_h
#include "slice.hh"
#include <ctime>
#include <stdint.h>
#include <unordered_map>
#include <vector>


namespace fleece {

    class array;
    class dict;
    class dictIterator;
        

    /* Types of values */
    enum valueType : uint8_t {
        kNull = 0,
        kBoolean,
        kNumber,
        kDate,
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

        /** Returns true if the contents of the slice might be a value (just based on 1st byte) */
        static bool mayBeValue(slice s)         {return s.size > 0 && s[0] <= kDictCode;}

        valueType type() const;

        bool asBool() const;
        int64_t asInt() const;
        uint64_t asUnsigned() const;
        double asDouble() const;
        bool isInteger() const                  {return _typeCode >= kInt1Code
                                                     && _typeCode <= kUInt64Code;}
        bool isUnsigned() const                 {return _typeCode == kUInt64Code;}

        std::time_t asDate() const;
        std::string asDateString() const;

        typedef std::vector<std::string> stringTable;

        /** Returns the exact contents of a string, data, or raw number.
            If the string is extern, a stringTable must be provided. */
        slice asString(const stringTable* externStrings =NULL) const;

        bool isExternString() const             {return _typeCode == kExternStringRefCode;}
        bool isSharedString() const             {return _typeCode == kSharedStringCode
                                                     || _typeCode == kSharedStringRefCode;}

        /** If this is an extern string, returns its external identifier.
            If this is a shared string, returns an opaque value identifying it; all instances of
            the same shared string in the same document will have the same identifier. */
        uint64_t stringToken() const;

        const array* asArray() const;
        const dict* asDict() const;

        /** Converts any non-collection type (except externString) to string form. */
        std::string toString() const;

        /** Writes a JSON representation to an ostream. */
        void writeJSON(std::ostream&, const std::vector<std::string> *externStrings) const;
        /** Returns a JSON representation. */
        std::string toJSON(const std::vector<std::string> *externStrings =NULL) const;

#ifdef __OBJC__
        id asNSObject(NSArray* externStrings =nil) const;
        id asNSObject(NSMapTable *sharedStrings, NSArray* externStrings =nil) const;
        static NSMapTable* createSharedStringsTable();
#endif

    protected:
        // The actual type codes used in the encoded data:
        enum typeCode : uint8_t {
            kNullCode = 0,
            kFalseCode, kTrueCode,
            kInt1Code, // kInt2Code, kInt3Code, ...
            kInt8Code = kInt1Code + 7,
            kUInt64Code,
            kFloat32Code, kFloat64Code,
            kRawNumberCode,
            kDateCode,
            kStringCode, kSharedStringCode, kSharedStringRefCode, kExternStringRefCode,
            kDataCode,
            kArrayCode,
            kDictCode,
        };
        
        typeCode _typeCode;
        uint8_t _paramStart[1];

        uint32_t getParam() const;
        uint32_t getParam(const uint8_t* &after) const;
        uint64_t getParam64(const uint8_t* &after) const;

        const value* next() const;

        static bool validate(const void* start, slice&);
        friend class encoder;
        friend class array;
        friend class dict;
    };


    /** A value that's an array. */
    class array : public value {
    public:
        uint32_t count() const                      {return getParam();}
        const value* first() const;

        class iterator {
        public:
            iterator(const array* a)                {_value = a->first(_count);}
            const class value* value() const        {return _value;}
            operator const class value* const ()    {return _value;}
            const class value* operator-> ()        {return _value;}

            explicit operator bool() const          {return _count > 0;}
            iterator& operator++();

        private:
            const class value* _value;
            uint32_t _count;
        };

    private:
        const value* first(uint32_t &count) const;
    };


    /** A value that's a dictionary/map */
    class dict : public value {
    public:
        class iterator {
        public:
            iterator(const dict*);
            const value* key() const        {return _key;}
            const value* value() const      {return _value;}

            explicit operator bool() const  {return _count > 0;}
            iterator& operator++();

        private:
            const class value* _value;
            const class value* _key;
            uint32_t _count;
        };

        uint32_t count() const              {return getParam();}

        /** Looks up the value for a key. */
        const value* get(slice key,
                         const stringTable* externStrings =NULL) const
                                            {return get(key, hashCode(key), externStrings);}

        /** Looks up the value for a key whose hashCode you already know. Slightly faster. */
        const value* get(slice key,
                         uint16_t hashCode,
                         const stringTable* externStrings =NULL) const;

        static uint16_t hashCode(slice);

    private:
        const value* firstKey(uint32_t &count) const;
        friend class value;
    };

}

#endif
