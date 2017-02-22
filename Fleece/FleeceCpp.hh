//
//  FleeceCpp.hh
//  Fleece
//
//  Created by Jens Alfke on 2/15/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#ifndef _FLEECE_H
#include "Fleece.h"
#endif
#include <string>

namespace fleeceapi {
    class Array;
    class Dict;


    static inline FLString FLStr(const std::string &s) {
        return {s.data(), s.size()};
    }

    static inline std::string asstring(FLString s) {
        return std::string((char*)s.buf, s.size);
    }

    static inline std::string asstring(FLStringResult &&s) {
        auto str = std::string((char*)s.buf, s.size);
        FLSliceResult_Free(s);
        return str;
    }

    static inline bool operator== (FLSlice s1, FLSlice s2) {return FLSlice_Equal(s1, s2);}
    static inline bool operator!= (FLSlice s1, FLSlice s2) {return !(s1 == s2);}

    static inline bool operator== (FLSliceResult sr, FLSlice s) {return (FLSlice)sr == s;}
    static inline bool operator!= (FLSliceResult sr, FLSlice s) {return !(sr ==s);}
    

    class Value {
    public:
        static Value fromData(FLSlice data)          {return Value(FLValue_FromData(data));}
        static Value fromTrustedData(FLSlice data)   {return Value(FLValue_FromTrustedData(data));}
        
        Value();
        Value(FLValue v)                                :_val(v) { }
        operator FLValue const ()                       {return _val;}

        inline FLValueType type() const;
        inline bool isInteger() const;
        inline bool isUnsigned() const;
        inline bool isDouble() const;

        inline bool asBool() const;
        inline int64_t asInt() const;
        inline uint64_t asUnsigned() const;
        inline float asFloat() const;
        inline double asDouble() const;
        inline FLString asString() const;
        inline FLSlice asData() const;
        inline Array asArray() const;
        inline Dict asDict() const;

        inline std::string asstring() const             {return ::fleeceapi::asstring(asString());}

        inline FLStringResult toString() const;
        inline FLStringResult toJSON() const;
        inline FLStringResult toJSON5() const;

        explicit operator bool() const                  {return _val != nullptr;}
        bool operator! () const                         {return _val == nullptr;}

    protected:
        const ::FLValue _val {nullptr};
    };


    class valueptr {  // A bit of ugly glue used to make Array/Dict iterator's operator-> work
    public:
        explicit valueptr(Value v)                      :_value(v) { }
        Value* operator-> ()                            {return &_value;}
    private:
        Value _value;
    };


    class Array : public Value {
    public:
        Array();
        Array(FLArray a)                                :Value((FLValue)a) { }
        operator FLArray () const                       {return (FLArray)_val;}

        inline uint32_t count() const;
        inline bool empty() const                       {return count() == 0;}
        inline Value get(uint32_t index) const;

        inline Value operator[] (uint32_t index) const  {return get(index);}

        class iterator : private FLArrayIterator {
        public:
            inline iterator(Array);
            inline Value value() const;
            inline bool next();
            inline valueptr operator -> () const        {return valueptr(value());}
            inline Value operator * () const            {return value();}
            inline explicit operator bool() const       {return (bool)value();}
            inline iterator& operator++ ()              {next(); return *this;}
            inline bool operator!= (const iterator&)    {return value() != nullptr;}
        private:
            iterator() { }
            friend class Array;
        };

        // begin/end are just provided so you can use the C++11 "for (Value v : array)" syntax.
        inline iterator begin() const                   {return iterator(*this);}
        inline iterator end() const                     {return iterator();}
    };


    class Dict : public Value {
    public:
        Dict();
        Dict(FLDict d)                                  :Value((FLValue)d) { }
        operator FLDict () const                        {return (FLDict)_val;}

        inline uint32_t count() const;
        inline bool empty() const                       {return count() == 0;}

        inline Value get(FLString key) const;
        inline Value get(FLString key, FLSharedKeys sk) const;

        inline Value get(const char* key) const                   {return get(FLStr(key));}
        inline Value get(const char *key, FLSharedKeys sk) const  {return get(FLStr(key), sk);}

        inline Value operator[] (FLString key) const    {return get(key);}
        inline Value operator[] (const char *key) const {return get(key);}

        class Key {
        public:
            Key(FLSlice strng, bool cachePointers =false);
            Key(FLSlice string, FLSharedKeys sharedKeys);
            inline FLString string() const;
            operator FLString() const                   {return string();}
        private:
            FLDictKey _key;
            friend class Dict;
        };

        inline Value get(Key &key) const;
        inline Value operator[] (Key &key) const        {return get(key);}

        class iterator : private FLDictIterator {
        public:
            inline iterator(Dict);
            inline Value key() const;
            inline Value value() const;
            inline bool next();

            inline valueptr operator -> () const        {return valueptr(value());}
            inline Value operator * () const            {return value();}
            inline explicit operator bool() const       {return (bool)value();}
            inline iterator& operator++ ()              {next(); return *this;}
            inline bool operator!= (const iterator&)    {return value() != nullptr;}
        private:
            iterator() { }
            friend class Dict;
        };

        // begin/end are just provided so you can use the C++11 "for (Value v : dict)" syntax.
        inline iterator begin() const                   {return iterator(*this);}
        inline iterator end() const                     {return iterator();}
    };


    class Encoder {
    public:
        Encoder()                                       :_enc(FLEncoder_New()) { }
        explicit Encoder(FLEncoder enc)                 :_enc(enc) { }

        explicit Encoder(FLEncoderFormat format,
                         size_t reserveSize =0,
                         bool uniqueStrings =true,
                         bool sortKeys =true)
        :_enc(FLEncoder_NewWithOptions(format, reserveSize, uniqueStrings, sortKeys))
        { }

        ~Encoder()                                      {FLEncoder_Free(_enc);}

        void setSharedKeys(FLSharedKeys sk)             {FLEncoder_SetSharedKeys(_enc, sk);}

        static FLSliceResult convertJSON(FLSlice json, FLError *error) {
            return FLData_ConvertJSON(json, error);
        }

        operator ::FLEncoder ()                         {return _enc;}

        inline bool writeNull();
        inline bool writeBool(bool);
        inline bool writeInt(int64_t);
        inline bool writeUInt(int64_t);
        inline bool writeFloat(float);
        inline bool writeDouble(double);
        inline bool writeString(FLString);
        inline bool writeString(const char *s)          {return writeString(FLStr(s));}
        inline bool writeString(std::string s)          {return writeString(FLStr(s));}
        inline bool writeData(FLSlice);
        inline bool writeValue(Value);
        inline bool convertJSON(FLSlice);

        inline bool beginArray(size_t reserveCount =0);
        inline bool endArray();
        inline bool beginDict(size_t reserveCount =0);
        inline bool writeKey(FLString);
        inline bool endDict();

        inline size_t bytesWritten() const;

        inline FLSliceResult finish(FLError* =nullptr);
        inline void reset();

        inline FLError error() const;
        inline const char* errorMessage() const;

        //////// "<<" convenience operators;

        // Note: overriding <<(bool) would be dangerous due to implicit conversion
        Encoder& operator<< (long long i)           {writeInt(i); return *this;}
        Encoder& operator<< (unsigned long long i)  {writeUInt(i); return *this;}
        Encoder& operator<< (long i)                {writeInt(i); return *this;}
        Encoder& operator<< (unsigned long i)       {writeUInt(i); return *this;}
        Encoder& operator<< (int i)                 {writeInt(i); return *this;}
        Encoder& operator<< (unsigned int i)        {writeUInt(i); return *this;}
        Encoder& operator<< (double d)              {writeDouble(d); return *this;}
        Encoder& operator<< (float f)               {writeFloat(f); return *this;}
        Encoder& operator<< (FLString s)            {writeString(s); return *this;}
        Encoder& operator<< (const  char *str)      {writeString(str); return *this;}
        Encoder& operator<< (const std::string &s)  {writeString(s); return *this;}
        Encoder& operator<< (Value v)               {writeValue(v); return *this;}

    protected:
        Encoder(const Encoder&) =delete;
        Encoder& operator=(const Encoder&) =delete;

        const FLEncoder _enc;
    };

    class JSONEncoder : public Encoder {
    public:
        JSONEncoder()                           :Encoder(kFLEncodeJSON) { }
        inline bool writeRaw(FLSlice raw)       {return FLEncoder_WriteRaw(_enc, raw);}
    };

    class JSON5Encoder : public Encoder {
    public:
        JSON5Encoder()                          :Encoder(kFLEncodeJSON5) { }
    };



    inline FLValueType Value::type() const      {return FLValue_GetType(_val);}
    inline bool Value::isInteger() const        {return FLValue_IsInteger(_val);}
    inline bool Value::isUnsigned() const       {return FLValue_IsUnsigned(_val);}
    inline bool Value::isDouble() const         {return FLValue_IsDouble(_val);}

    inline bool Value::asBool() const           {return FLValue_AsBool(_val);}
    inline int64_t Value::asInt() const         {return FLValue_AsInt(_val);}
    inline uint64_t Value::asUnsigned() const   {return FLValue_AsUnsigned(_val);}
    inline float Value::asFloat() const         {return FLValue_AsFloat(_val);}
    inline double Value::asDouble() const       {return FLValue_AsDouble(_val);}
    inline FLString Value::asString() const     {return FLValue_AsString(_val);}
    inline FLSlice Value::asData() const        {return FLValue_AsData(_val);}
    inline Array Value::asArray() const         {return FLValue_AsArray(_val);}
    inline Dict Value::asDict() const           {return FLValue_AsDict(_val);}

    inline FLStringResult Value::toString() const {return FLValue_ToString(_val);}
    inline FLStringResult Value::toJSON() const {return FLValue_ToJSON(_val);}
    inline FLStringResult Value::toJSON5() const{return FLValue_ToJSON5(_val);}

    inline uint32_t Array::count() const        {return FLArray_Count(*this);}
    inline Value Array::get(uint32_t i) const   {return FLArray_Get(*this, i);}

    inline Array::iterator::iterator(Array a)   {FLArrayIterator_Begin(a, this);}
    inline Value Array::iterator::value() const {return FLArrayIterator_GetValue(this);}
    inline bool Array::iterator::next()         {return FLArrayIterator_Next(this);}

    inline uint32_t Dict::count() const         {return FLDict_Count(*this);}
    inline Value Dict::get(FLSlice key) const   {return FLDict_Get(*this, key);}
    inline Value Dict::get(FLSlice key, FLSharedKeys sk) const {return FLDict_GetSharedKey(*this, key, sk);}
    inline Value Dict::get(Dict::Key &key) const{return FLDict_GetWithKey(*this, &key._key);}

    inline Dict::Key::Key(FLSlice s, bool c)    :_key(FLDictKey_Init(s, c)) { }
    inline Dict::Key::Key(FLSlice s, FLSharedKeys sk) :_key(FLDictKey_InitWithSharedKeys(s, sk)) { }
    inline FLString Dict::Key::string() const   {return FLDictKey_GetString(&_key);}

    inline Dict::iterator::iterator(Dict d)     {FLDictIterator_Begin(d, this);}
    inline Value Dict::iterator::key() const    {return FLDictIterator_GetKey(this);}
    inline Value Dict::iterator::value() const  {return FLDictIterator_GetValue(this);}
    inline bool Dict::iterator::next()          {return FLDictIterator_Next(this);}

    inline bool Encoder::writeNull()            {return FLEncoder_WriteNull(_enc);}
    inline bool Encoder::writeBool(bool b)      {return FLEncoder_WriteBool(_enc, b);}
    inline bool Encoder::writeInt(int64_t n)    {return FLEncoder_WriteInt(_enc, n);}
    inline bool Encoder::writeUInt(int64_t n)   {return FLEncoder_WriteUInt(_enc, n);}
    inline bool Encoder::writeFloat(float n)    {return FLEncoder_WriteFloat(_enc, n);}
    inline bool Encoder::writeDouble(double n)  {return FLEncoder_WriteDouble(_enc, n);}
    inline bool Encoder::writeString(FLString s){return FLEncoder_WriteString(_enc, s);}
    inline bool Encoder::writeData(FLSlice data){return FLEncoder_WriteData(_enc, data);}
    inline bool Encoder::writeValue(Value v)    {return FLEncoder_WriteValue(_enc, v);}
    inline bool Encoder::convertJSON(FLSlice j) {return FLEncoder_ConvertJSON(_enc, j);}
    inline bool Encoder::beginArray(size_t rsv) {return FLEncoder_BeginArray(_enc, rsv);}
    inline bool Encoder::endArray()             {return FLEncoder_EndArray(_enc);}
    inline bool Encoder::beginDict(size_t rsv)  {return FLEncoder_BeginDict(_enc, rsv);}
    inline bool Encoder::writeKey(FLString key) {return FLEncoder_WriteKey(_enc, key);}
    inline bool Encoder::endDict()              {return FLEncoder_EndDict(_enc);}
    inline size_t Encoder::bytesWritten() const {return FLEncoder_BytesWritten(_enc);}
    inline FLSliceResult Encoder::finish(FLError* err) {return FLEncoder_Finish(_enc, err);}
    inline void Encoder::reset()                {return FLEncoder_Reset(_enc);}
    inline FLError Encoder::error() const       {return FLEncoder_GetError(_enc);}
    inline const char* Encoder::errorMessage() const {return FLEncoder_GetErrorMessage(_enc);}
}
