//
//  Encoder.hh
//  Fleece
//
//  Created by Jens Alfke on 1/26/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#ifndef __Fleece__DataWriter__
#define __Fleece__DataWriter__

#include "Value.hh"
#include "Writer.hh"
#include <ctime>
#include <iostream>
#include <unordered_map>


namespace fleece {

    class encoder {
    public:
        encoder(Writer&,
                   value::stringTable *externStrings = NULL,
                   uint32_t maxExternStrings = 0);

        void enableSharedStrings(bool e)            {_enableSharedStrings = e;}

        void writeNull();
        void writeBool (bool);

        void writeInt(int64_t);
        void writeUInt(uint64_t);
        void writeFloat(float);
        void writeDouble(double);
        void writeRawNumber(slice);
        void writeRawNumber(std::string str)        {writeRawNumber(slice(str));}

        void writeDate(std::time_t);

        void writeString(std::string, bool canAddExtern =false);
        void writeString(slice, bool canAddExtern =false);
        void writeExternString(uint32_t externRef);

        void writeData(slice);

        void beginArray(uint32_t count);
        void endArray()                             {popState();}

        void beginDict(uint32_t count);
        void writeKey(std::string, bool canAddExtern =true);
        void writeKey(slice, bool canAddExtern =true);
        void writeExternKey(uint32_t externRef, uint16_t hash);
        void endDict();

        // Note: overriding <<(bool) would be dangerous due to implicit conversion
        encoder& operator<< (int64_t i)          {writeInt(i); return *this;}
        encoder& operator<< (double d)           {writeDouble(d); return *this;}
        encoder& operator<< (float f)            {writeFloat(f); return *this;}
        encoder& operator<< (std::string str)    {writeString(str); return *this;}
        encoder& operator<< (slice s)            {writeString(s); return *this;}

#ifdef __OBJC__
        void write(id);
#endif

    private:
        void addTypeCode(value::typeCode code)      {_out << code; ++_state->i;}
        void addUVarint(uint64_t);
        bool canShareString(size_t len) const;

        struct state {
            uint32_t count;
            uint32_t i;
            size_t indexPos;
            uint16_t* hashes;
        };

        void pushState();
        void popState();
        void pushCount(uint32_t count);

        typedef std::unordered_map<std::string, uint32_t> stringLookupTable;

        Writer& _out;
        state* _state;
        std::vector<state> _states;
        bool _enableSharedStrings;
        stringLookupTable _sharedStrings;
        stringLookupTable _externStringsLookup;
        value::stringTable* _externStrings;
        uint32_t _maxExternStrings;
    };


}

#endif /* defined(__Fleece__DataWriter__) */
