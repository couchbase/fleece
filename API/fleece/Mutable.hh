//
// Mutable.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "fleece/Fleece.hh"

namespace fleece {

    /** Equivalent to Value except that, if it holds a mutable Array/Dict, it will retain the
        reference so it won't be freed. */
    class RetainedValue : public Value {
    public:
        RetainedValue()                           { }
        RetainedValue(FLValue v)                  :Value(FLValue_Retain(v)) { }
        RetainedValue(Value &v)                   :Value(FLValue_Retain(v)) { }
        RetainedValue(RetainedValue &&v)          :Value(v) {v._val = nullptr;}
        ~RetainedValue()                          {FLValue_Release(_val);}

        RetainedValue& operator= (const Value &v) {
            FLValue_Retain(v);
            FLValue_Release(_val);
            _val = v;
            return *this;
        }

        RetainedValue& operator= (RetainedValue &&v) {
            if (v._val != _val) {
                FLValue_Release(_val);
                _val = v._val;
            }
            return *this;
        }
    };


    /** A mutable form of Array. Its storage lives in the heap, not in the (immutable) Fleece
        document. It can be used to make a changed form of a document, which can then be
        encoded to a new Fleece document. */
    class MutableArray : public Array {
    public:
        static MutableArray newArray()          {return MutableArray(FLMutableArray_New(), true);}
        static MutableArray newArray(Array a)   {return MutableArray(FLArray_MutableCopy(a), true);}

        MutableArray()                          :Array() { }
        MutableArray(FLMutableArray a)          :Array((FLArray)a) {FLMutableArray_Retain(*this);}
        MutableArray(const MutableArray &a)     :Array((FLArray)a) {FLMutableArray_Retain(*this);}
        MutableArray(MutableArray &&a)          :Array((FLArray)a) {a._val = nullptr;}
        ~MutableArray()                         {FLMutableArray_Release(*this);}

        operator FLMutableArray () const        {return (FLMutableArray)_val;}

        MutableArray& operator= (const MutableArray &a) {
            FLMutableArray_Retain(a);
            FLMutableArray_Release(*this);
            _val = a._val;
            return *this;
        }

        MutableArray& operator= (MutableArray &&a) {
            if (a._val != _val) {
                FLMutableArray_Release(*this);
                _val = a._val;
            }
            return *this;
        }

        Array source() const                    {return FLMutableArray_GetSource(*this);}
        bool isChanged() const                  {return FLMutableArray_IsChanged(*this);}

        void remove(uint32_t first, uint32_t n) {FLMutableArray_Remove(*this, first, n);}
        void resize(uint32_t size)              {FLMutableArray_Resize(*this, size);}

        void setNull(uint32_t i)                {FLMutableArray_SetNull(*this, i);}
        void set(uint32_t i, bool v)            {FLMutableArray_SetBool(*this, i, v);}
        void set(uint32_t i, int64_t v)         {FLMutableArray_SetInt(*this, i, v);}
        void set(uint32_t i, uint64_t v)        {FLMutableArray_SetUInt(*this, i, v);}
        void set(uint32_t i, float v)           {FLMutableArray_SetFloat(*this, i, v);}
        void set(uint32_t i, double v)          {FLMutableArray_SetDouble(*this, i, v);}
        void set(uint32_t i, FLString v)        {FLMutableArray_SetString(*this, i, v);}
        void setData(uint32_t i, FLSlice v)     {FLMutableArray_SetData(*this, i, v);}
        void set(uint32_t i, Value v)           {FLMutableArray_SetValue(*this, i, v);}

        void appendNull()                       {FLMutableArray_AppendNull(*this);}
        void append(bool v)                     {FLMutableArray_AppendBool(*this, v);}
        void append(int64_t v)                  {FLMutableArray_AppendInt(*this, v);}
        void append(uint64_t v)                 {FLMutableArray_AppendUInt(*this, v);}
        void append(float v)                    {FLMutableArray_AppendFloat(*this, v);}
        void append(double v)                   {FLMutableArray_AppendDouble(*this, v);}
        void append(FLString v)                 {FLMutableArray_AppendString(*this, v);}
        void appendData(FLSlice v)              {FLMutableArray_AppendData(*this, v);}
        void append(Value v)                    {FLMutableArray_AppendValue(*this, v);}

        inline MutableArray getMutableArray(uint32_t i);
        inline MutableDict getMutableDict(uint32_t i);

    private:
        MutableArray(FLMutableArray a, bool)     :Array((FLArray)a) {}
    };


    /** A mutable form of Dict. Its storage lives in the heap, not in the (immutable) Fleece
        document. It can be used to make a changed form of a document, which can then be
        encoded to a new Fleece document. */
    class MutableDict : public Dict {
    public:
        static MutableDict newDict()            {return MutableDict(FLMutableDict_New(), true);}
        static MutableDict newDict(Dict d)      {return MutableDict(FLDict_MutableCopy(d), true);}

        MutableDict()                           :Dict() { }
        MutableDict(FLMutableDict d)            :Dict((FLDict)d) {FLMutableDict_Retain(*this);}
        MutableDict(const MutableDict &d)       :Dict((FLDict)d) {FLMutableDict_Retain(*this);}
        MutableDict(MutableDict &&d)            :Dict((FLDict)d) {d._val = nullptr;}
        ~MutableDict()                          {FLMutableDict_Release(*this);}

        operator FLMutableDict () const         {return (FLMutableDict)_val;}

        MutableDict& operator= (const MutableDict &d) {
            FLMutableDict_Retain(d);
            FLMutableDict_Release(*this);
            _val = d._val;
            return *this;
        }

        MutableDict& operator= (MutableDict &&d) {
            if (d._val != _val) {
                FLMutableDict_Release(*this);
                _val = d._val;
            }
            return *this;
        }

        Dict source() const                     {return FLMutableDict_GetSource(*this);}
        bool isChanged() const                  {return FLMutableDict_IsChanged(*this);}

        void remove(FLString key)               {FLMutableDict_Remove(*this, key);}

        void setNull(FLString k)                {FLMutableDict_SetNull(*this, k);}
        void set(FLString k, bool v)            {FLMutableDict_SetBool(*this, k, v);}
        void set(FLString k, int64_t v)         {FLMutableDict_SetInt(*this, k, v);}
        void set(FLString k, uint64_t v)        {FLMutableDict_SetUInt(*this, k, v);}
        void set(FLString k, float v)           {FLMutableDict_SetFloat(*this, k, v);}
        void set(FLString k, double v)          {FLMutableDict_SetDouble(*this, k, v);}
        void set(FLString k, FLString v)        {FLMutableDict_SetString(*this, k, v);}
        void setData(FLString k, FLSlice v)     {FLMutableDict_SetData(*this, k, v);}
        void set(FLString k, Value v)           {FLMutableDict_SetValue(*this, k, v);}

        inline MutableArray getMutableArray(FLString key);
        inline MutableDict getMutableDict(FLString key);

    private:
        MutableDict(FLMutableDict d, bool)      :Dict((FLDict)d) {}
    };

    

    //////// IMPLEMENTATION GUNK:

    inline MutableArray MutableArray::getMutableArray(uint32_t i)
                                                {return FLMutableArray_GetMutableArray(*this, i);}
    inline MutableDict MutableArray::getMutableDict(uint32_t i)
                                                {return FLMutableArray_GetMutableDict(*this, i);}
    inline MutableArray MutableDict::getMutableArray(FLString key)
                                                {return FLMutableDict_GetMutableArray(*this, key);}
    inline MutableDict MutableDict::getMutableDict(FLString key)
                                                {return FLMutableDict_GetMutableDict(*this, key);}

    inline MutableArray Array::asMutable() const {
        return MutableArray(FLMutableArray_Retain(FLArray_AsMutable(*this)));
    }

    inline MutableDict Dict::asMutable() const {
        return MutableDict(FLMutableDict_Retain(FLDict_AsMutable(*this)));
    }

}
