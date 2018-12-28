//
// Fleece.hh
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
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
#ifndef _FLEECE_HH
#define _FLEECE_HH
#ifndef _FLEECE_H
#include "fleece/Fleece.h"
#endif
#include "fleece/slice.hh"
#include <string>

namespace fleece {
    class Array;
    class Dict;
    class MutableArray;
    class MutableDict;
    class KeyPath;
    class SharedKeys;
    class Doc;


    static inline bool operator== (FLSlice s1, FLSlice s2) {return FLSlice_Equal(s1, s2);}
    static inline bool operator!= (FLSlice s1, FLSlice s2) {return !(s1 == s2);}

    static inline bool operator== (FLSliceResult sr, FLSlice s) {return (FLSlice)sr == s;}
    static inline bool operator!= (FLSliceResult sr, FLSlice s) {return !(sr ==s);}


    /** A Fleece data value. Its subclasses are Array and Dict; Value itself is for scalars. */
    class Value {
    public:
        Value()                                         { }
        Value(FLValue v)                                :_val(v) { }
        operator FLValue() const                        {return _val;}

        static Value null()                             {return Value(kFLNullValue);}

        inline FLValueType type() const;
        inline bool isInteger() const;
        inline bool isUnsigned() const;
        inline bool isDouble() const;

        inline bool asBool() const;
        inline int64_t asInt() const;
        inline uint64_t asUnsigned() const;
        inline float asFloat() const;
        inline double asDouble() const;
        inline slice asString() const;
        inline FLTimestamp asTimestamp() const;
        inline slice asData() const;
        inline Array asArray() const;
        inline Dict asDict() const;

        inline std::string asstring() const             {return asString().asString();}

        inline alloc_slice toString() const;
        inline alloc_slice toJSON() const;
        inline alloc_slice toJSON5() const;

        inline alloc_slice toJSON(bool json5 =false, bool canonical =false);
        inline std::string toJSONString()               {return std::string(toJSON());}

        explicit operator bool() const                  {return _val != nullptr;}
        bool operator! () const                         {return _val == nullptr;}
        bool operator== (Value v) const                 {return _val == v._val;}
        bool operator== (FLValue v) const               {return _val == v;}
        bool operator!= (Value v) const                 {return _val != v._val;}
        bool operator!= (FLValue v) const               {return _val != v;}

        bool isEqual(Value v) const                     {return FLValue_IsEqual(_val, v);}

        Value& operator= (Value v)                      {_val = v._val; return *this;}
        Value& operator= (nullptr_t)                    {_val = nullptr; return *this;}

        inline Value operator[] (const KeyPath &kp) const;

        inline Doc findDoc() const;

        static Value fromData(fleece::slice data, FLTrust t =kFLUntrusted)
                                                        {return FLValue_FromData(data,t);}

#ifdef __OBJC__
        inline id asNSObject(NSMapTable *sharedStrings =nil) const {
            return FLValue_GetNSObject(_val, sharedStrings);
        }
#endif

        // Disallowed because the mutable value would be released, which might free it:
        Value(MutableArray&&) =delete;
        Value& operator= (MutableArray&&) =delete;
        Value(MutableDict&&) =delete;
        Value& operator= (MutableDict&&) =delete;

    protected:
        ::FLValue _val {nullptr};
    };


    class valueptr {  // A bit of ugly glue used to make Array/Dict iterator's operator-> work
    public:
        explicit valueptr(Value v)                      :_value(v) { }
        Value* operator-> ()                            {return &_value;}
    private:
        Value _value;
    };


    /** An array of Fleece values. */
    class Array : public Value {
    public:
        Array()                                         :Value() { }
        Array(FLArray a)                                :Value((FLValue)a) { }
        operator FLArray () const                       {return (FLArray)_val;}

        static Array emptyArray()                       {return Array(kFLEmptyArray);}

        inline uint32_t count() const;
        inline bool empty() const;
        inline Value get(uint32_t index) const;

        inline Value operator[] (int index) const       {return get(index);}
        inline Value operator[] (const KeyPath &kp) const {return Value::operator[](kp);}

        Array& operator= (Array a)                      {_val = a._val; return *this;}
        Value& operator= (Value v)                      =delete;

        inline MutableArray asMutable() const;

        class iterator : private FLArrayIterator {
        public:
            inline iterator(Array);
            inline iterator(const FLArrayIterator &i)   :FLArrayIterator(i) { }
            inline Value value() const;
            inline uint32_t count() const               {return FLArrayIterator_GetCount(this);}
            inline bool next();
            inline valueptr operator -> () const        {return valueptr(value());}
            inline Value operator * () const            {return value();}
            inline explicit operator bool() const       {return (bool)value();}
            inline iterator& operator++ ()              {next(); return *this;}
            inline bool operator!= (const iterator&)    {return value() != nullptr;}
            inline Value operator[] (unsigned n) const  {return FLArrayIterator_GetValueAt(this,n);}
        private:
            iterator() { }
            friend class Array;
        };

        // begin/end are just provided so you can use the C++11 "for (Value v : array)" syntax.
        inline iterator begin() const                   {return iterator(*this);}
        inline iterator end() const                     {return iterator();}

        // Disallowed because the MutableArray would be released, which might free it:
        Array(MutableArray&&) =delete;
        Array& operator= (MutableArray&&) =delete;
    };


    /** A mapping of strings to values. */
    class Dict : public Value {
    public:
        Dict()                                          :Value() { }
        Dict(FLDict d)                                  :Value((FLValue)d) { }
        operator FLDict () const                        {return (FLDict)_val;}

        static Dict emptyDict()                         {return Dict(kFLEmptyDict);}

        inline uint32_t count() const;
        inline bool empty() const;

        inline Value get(slice key) const;

        inline Value get(const char* key) const         {return get(slice(key));}

        inline Value operator[] (slice key) const       {return get(key);}
        inline Value operator[] (const char *key) const {return get(key);}
        inline Value operator[] (const KeyPath &kp) const {return Value::operator[](kp);}

        Dict& operator= (Dict d)                        {_val = d._val; return *this;}
        Value& operator= (Value v)                      =delete;

        inline MutableDict asMutable() const;

        /** An efficient key for a Dict. */
        class Key {
        public:
            // Warning: the input string's memory MUST remain valid for as long as the Key is in
            // use! (The Key stores a pointer to the string, but does not copy it.)
            explicit Key(slice string);
            inline slice string() const;
            operator slice() const                      {return string();}
        private:
            FLDictKey _key;
            friend class Dict;
        };

        inline Value get(Key &key) const;
        inline Value operator[] (Key &key) const        {return get(key);}

        class iterator : private FLDictIterator {
        public:
            inline iterator(Dict);
            inline iterator(const FLDictIterator &i)   :FLDictIterator(i) { }
            inline uint32_t count() const               {return FLDictIterator_GetCount(this);}
            inline Value key() const;
            inline slice keyString() const;
            inline Value value() const;
            inline bool next();

            inline valueptr operator -> () const        {return valueptr(value());}
            inline Value operator * () const            {return value();}
            inline explicit operator bool() const       {return (bool)value();}
            inline iterator& operator++ ()              {next(); return *this;}
            inline bool operator!= (const iterator&) const    {return value() != nullptr;}

#ifdef __OBJC__
            inline NSString* keyAsNSString(NSMapTable *sharedStrings) const
                                    {return FLDictIterator_GetKeyAsNSString(this, sharedStrings);}
#endif
        private:
            iterator() { }
            friend class Dict;
        };

        // begin/end are just provided so you can use the C++11 "for (Value v : dict)" syntax.
        inline iterator begin() const                   {return iterator(*this);}
        inline iterator end() const                     {return iterator();}

        // Disallowed because the MutableDict would be released, which might free it:
        Dict(MutableDict&&) =delete;
        Dict& operator= (MutableDict&&) =delete;
    };


    /** Describes a location in a Fleece object tree, as a path from the root that follows
        dictionary properties and array elements.
        Similar to a JSONPointer or an Objective-C KeyPath, but simpler (so far.)
        It looks like "foo.bar[2][-3].baz" -- that is, properties prefixed with a ".", and array
        indexes in brackets. (Negative indexes count from the end of the array.)
        A leading JSONPath-like "$." is allowed but ignored.
        A '\' can be used to escape a special character ('.', '[' or '$') at the start of a
        property name (but not yet in the middle of a name.) */
    class KeyPath {
    public:
        KeyPath(slice spec, FLError *err)               :_path(FLKeyPath_New(spec, err)) { }
        ~KeyPath()                                      {FLKeyPath_Free(_path);}
        explicit operator bool() const                  {return _path != nullptr;}

        static Value eval(slice specifier, Value root, FLError *error) {
            return FLKeyPath_EvalOnce(specifier, root, error);
        }

    private:
        KeyPath(const KeyPath&) =delete;
        KeyPath& operator=(const KeyPath&) =delete;
        friend class Value;

        FLKeyPath _path;
    };


    /** An iterator that traverses an entire value hierarchy, descending into Arrays and Dicts. */
    class DeepIterator {
    public:
        DeepIterator(Value v)                           :_i(FLDeepIterator_New(v)) { }
        ~DeepIterator()                                 {FLDeepIterator_Free(_i);}

        Value value() const                             {return FLDeepIterator_GetValue(_i);}
        slice key() const                               {return FLDeepIterator_GetKey(_i);}
        uint32_t index() const                          {return FLDeepIterator_GetIndex(_i);}

        size_t depth() const                            {return FLDeepIterator_GetDepth(_i);}
        alloc_slice pathString() const                  {return FLDeepIterator_GetPathString(_i);}
        alloc_slice JSONPointer() const                 {return FLDeepIterator_GetJSONPointer(_i);}

        void skipChildren()                             {FLDeepIterator_SkipChildren(_i);}
        bool next()                                     {return FLDeepIterator_Next(_i);}

        explicit operator bool() const                  {return value() != nullptr;}
        DeepIterator& operator++()                      {next(); return *this;}

    private:
        DeepIterator(const DeepIterator&) =delete;

        FLDeepIterator _i;
    };


    /** Keeps track of a set of dictionary keys that are stored in abbreviated (small integer) form.

        Encoders can be configured to use an instance of this, and will use it to abbreviate keys
        that are given to them as strings. (Note: This class is not thread-safe!) */
    class SharedKeys {
    public:
        SharedKeys()                                        :_sk(nullptr) { }
        SharedKeys(FLSharedKeys sk)                         :_sk(FLSharedKeys_Retain(sk)) { }
        SharedKeys(const SharedKeys &other) noexcept        :_sk(FLSharedKeys_Retain(other._sk)) { }
        SharedKeys(SharedKeys &&other) noexcept             :_sk(other._sk) {other._sk = nullptr;}
        ~SharedKeys()                                       {FLSharedKeys_Release(_sk);}

        static SharedKeys create()                          {return FLSharedKeys_Create();}
        static SharedKeys create(slice state)      {return FLSharedKeys_CreateFromStateData(state);}
        alloc_slice stateData() const                       {return FLSharedKeys_GetStateData(_sk);}

        operator FLSharedKeys() const                       {return _sk;}
        bool operator== (SharedKeys other) const            {return _sk == other._sk;}

    private:
        FLSharedKeys _sk {nullptr};
    };


    /** A container for Fleece data in memory. Every Value belongs to the Doc whose memory range
        contains it. The Doc keeps track of the SharedKeys used by its Dicts, and where to resolve
        external pointers to. */
    class Doc {
    public:
        Doc(fleece::alloc_slice fleeceData,
            FLTrust trust =kFLUntrusted,
            SharedKeys sk =nullptr,
            fleece::slice externDestination =fleece::nullslice) noexcept
        :_doc(FLDoc_FromResultData(FLSliceResult(fleeceData), trust, sk, externDestination))
        { }

        static inline Doc fromJSON(fleece::slice json, FLError *outError = nullptr);

        static alloc_slice dump(slice fleeceData)   {return FLData_Dump(fleeceData);}

        Doc()                                       :_doc(nullptr) { }
        Doc(FLDoc d, bool retain = true)            :_doc(d) {if (retain) FLDoc_Retain(_doc);}
        Doc(const Doc &other) noexcept              :_doc(FLDoc_Retain(other._doc)) { }
        Doc(Doc &&other) noexcept                   :_doc(other._doc) {other._doc=nullptr; }
        Doc& operator=(const Doc &other);
        Doc& operator=(Doc &&other);
        ~Doc()                                      {FLDoc_Release(_doc);}

        fleece::slice data() const                  {return FLDoc_GetData(_doc);}
        alloc_slice allocedData() const             {return FLDoc_GetAllocedData(_doc);}
        SharedKeys sharedKeys() const               {return FLDoc_GetSharedKeys(_doc);}

        Value root() const                          {return FLDoc_GetRoot(_doc);}
        explicit operator bool () const             {return root() != nullptr;}
        Array asArray() const                       {return root().asArray();}
        Dict asDict() const                         {return root().asDict();}

        operator Value () const                     {return root();}
        operator Dict () const                      {return asDict();}
        operator FLDict () const                    {return asDict();}

        Value operator[] (int index) const          {return asArray().get(index);}
        Value operator[] (slice key) const          {return asDict().get(key);}
        Value operator[] (const char *key) const    {return asDict().get(key);}
        Value operator[] (const KeyPath &kp) const  {return root().operator[](kp);}

        bool operator== (const Doc &d) const        {return _doc == d._doc;}

    private:
        friend class Value;
        explicit Doc(FLValue v)                     :_doc(FLValue_FindDoc(v)) { }

        FLDoc _doc;
    };


    /** Generates Fleece-encoded data. */
    class Encoder {
    public:
        Encoder()                                       :_enc(FLEncoder_New()) { }

        explicit Encoder(FLEncoderFormat format,
                         size_t reserveSize =0,
                         bool uniqueStrings =true)
        :_enc(FLEncoder_NewWithOptions(format, reserveSize, uniqueStrings))
        { }

        explicit Encoder(FILE *file,
                         bool uniqueStrings =true)
        :_enc(FLEncoder_NewWritingToFile(file, uniqueStrings))
        { }

        explicit Encoder(FLEncoder enc)                 :_enc(enc) { }

        void detach()                                   {_enc = nullptr;}
        
        ~Encoder()                                      {FLEncoder_Free(_enc);}

        void setSharedKeys(SharedKeys sk)               {FLEncoder_SetSharedKeys(_enc, sk);}

        inline void amend(slice base, bool reuseStrings =false, bool externPointers =false);
        slice base() const                              {return FLEncoder_GetBase(_enc);}

        void suppressTrailer()                          {FLEncoder_SuppressTrailer(_enc);}

        operator ::FLEncoder ()                         {return _enc;}

        inline bool writeNull();
        inline bool writeUndefined();
        inline bool writeBool(bool);
        inline bool writeInt(int64_t);
        inline bool writeUInt(uint64_t);
        inline bool writeFloat(float);
        inline bool writeDouble(double);
        inline bool writeString(slice);
        inline bool writeString(const char *s)          {return writeString(slice(s));}
        inline bool writeString(std::string s)          {return writeString(slice(s));}
        inline bool writeDateString(FLTimestamp, bool asUTC =true);
        inline bool writeData(slice);
        inline bool writeValue(Value);
        inline bool convertJSON(slice);

        inline bool beginArray(size_t reserveCount =0);
        inline bool endArray();
        inline bool beginDict(size_t reserveCount =0);
        inline bool writeKey(slice);
        inline bool writeKey(Value);
        inline bool endDict();

        template <class T>
        inline void write(slice key, T value)       {writeKey(key); *this << value;}

        inline void writeRaw(slice data)            {FLEncoder_WriteRaw(_enc, data);}

        inline size_t bytesWritten() const;
        inline size_t nextWritePos() const          {return FLEncoder_GetNextWritePos(_enc);}

        inline Doc finishDoc(FLError* =nullptr);
        inline alloc_slice finish(FLError* =nullptr);
        inline size_t finishItem()                  {return FLEncoder_FinishItem(_enc);}
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
        Encoder& operator<< (slice s)               {writeString(s); return *this;}
        Encoder& operator<< (const  char *str)      {writeString(str); return *this;}
        Encoder& operator<< (const std::string &s)  {writeString(s); return *this;}
        Encoder& operator<< (Value v)               {writeValue(v); return *this;}

        class keyref {
        public:
            keyref(Encoder &enc, slice key)         :_enc(enc), _key(key) { }
            template <class T>
            inline void operator= (T value)         {_enc.writeKey(_key); _enc << value;}
        private:
            Encoder &_enc;
            slice _key;
        };

        // This enables e.g. `enc["key"_sl] = 17`
        inline keyref operator[] (slice key)       {return keyref(*this, key);}

#ifdef __OBJC__
        bool writeNSObject(id obj)                 {return FLEncoder_WriteNSObject(_enc, obj);}
        Encoder& operator<< (id obj)               {writeNSObject(obj); return *this;}
        NSData* finish(NSError **err)              {return FLEncoder_FinishWithNSData(_enc, err);}
#endif

    protected:
        Encoder(const Encoder&) =delete;
        Encoder& operator=(const Encoder&) =delete;

        FLEncoder _enc;
    };


    /** Subclass of Encoder that generates JSON, not Fleece. */
    class JSONEncoder : public Encoder {
    public:
        JSONEncoder()                           :Encoder(kFLEncodeJSON) { }
        inline bool writeRaw(slice raw)       {return FLEncoder_WriteRaw(_enc, raw);}
    };

    /** Subclass of Encoder that generates JSON5 (an variant of JSON with cleaner syntax.) */
    class JSON5Encoder : public Encoder {
    public:
        JSON5Encoder()                          :Encoder(kFLEncodeJSON5) { }
    };


    /** Use this instead of Encoder if you don't own the FLEncoder. Its destructor does not
        free the underlying encoder object. */
    class SharedEncoder : public Encoder {
    public:
        explicit SharedEncoder(FLEncoder enc)   :Encoder(enc) { }

        ~SharedEncoder() {
            detach(); // prevents Encoder from freeing the FLEncoder
        }
    };


    /** Support for generating and applying JSON-format deltas/diffs between two Fleece values. */
    class JSONDelta {
    public:
        static inline fleece::alloc_slice create(Value old, Value nuu);
        static inline bool create(Value old, Value nuu, Encoder &jsonEncoder);

        static inline fleece::alloc_slice apply(Value old,
                                                fleece::slice jsonDelta,
                                                FLError *error);
        static inline bool apply(Value old,
                                 Value jsonDelta,
                                 Encoder &encoder);
    };


    //////// DEPRECATED:


    /** A Dict that manages its own storage. */
    class AllocedDict : public Dict, alloc_slice {
    public:
        AllocedDict()
        { }

        explicit AllocedDict(const alloc_slice &s)
        :Dict(FLValue_AsDict(FLValue_FromData(s, kFLUntrusted)))
        ,alloc_slice(s)
        { }

        explicit AllocedDict(fleece::slice s)
        :AllocedDict(alloc_slice(s)) { }

        AllocedDict(const AllocedDict &d)
        :Dict(d)
        ,alloc_slice((const alloc_slice&)d)
        { }

        const alloc_slice& data() const                 {return *this;}
        explicit operator bool () const                 {return Dict::operator bool();}

        // MI disambiguation:
        inline Value operator[] (slice key) const       {return Dict::get(key);}
        inline Value operator[] (const char *key) const {return Dict::get(key);}
    };


    //////// IMPLEMENTATION GUNK:

    inline FLValueType Value::type() const      {return FLValue_GetType(_val);}
    inline bool Value::isInteger() const        {return FLValue_IsInteger(_val);}
    inline bool Value::isUnsigned() const       {return FLValue_IsUnsigned(_val);}
    inline bool Value::isDouble() const         {return FLValue_IsDouble(_val);}

    inline bool Value::asBool() const           {return FLValue_AsBool(_val);}
    inline int64_t Value::asInt() const         {return FLValue_AsInt(_val);}
    inline uint64_t Value::asUnsigned() const   {return FLValue_AsUnsigned(_val);}
    inline float Value::asFloat() const         {return FLValue_AsFloat(_val);}
    inline double Value::asDouble() const       {return FLValue_AsDouble(_val);}
    inline FLTimestamp Value::asTimestamp() const {return FLValue_AsTimestamp(_val);}
    inline slice Value::asString() const     {return FLValue_AsString(_val);}
    inline slice Value::asData() const        {return FLValue_AsData(_val);}
    inline Array Value::asArray() const         {return FLValue_AsArray(_val);}
    inline Dict Value::asDict() const           {return FLValue_AsDict(_val);}

    inline alloc_slice Value::toString() const {return FLValue_ToString(_val);}
    inline alloc_slice Value::toJSON() const {return FLValue_ToJSON(_val);}
    inline alloc_slice Value::toJSON5() const{return FLValue_ToJSON5(_val);}

    inline alloc_slice Value::toJSON(bool json5, bool canonical) {
        return FLValue_ToJSONX(_val, json5, canonical);
    }

    inline Value Value::operator[] (const KeyPath &kp) const
                                                {return FLKeyPath_Eval(kp._path, _val);}
    inline Doc Value::findDoc() const           {return Doc(_val);}



    inline uint32_t Array::count() const        {return FLArray_Count(*this);}
    inline bool Array::empty() const            {return FLArray_IsEmpty(*this);}
    inline Value Array::get(uint32_t i) const   {return FLArray_Get(*this, i);}

    inline Array::iterator::iterator(Array a)   {FLArrayIterator_Begin(a, this);}
    inline Value Array::iterator::value() const {return FLArrayIterator_GetValue(this);}
    inline bool Array::iterator::next()         {return FLArrayIterator_Next(this);}

    inline uint32_t Dict::count() const         {return FLDict_Count(*this);}
    inline bool Dict::empty() const             {return FLDict_IsEmpty(*this);}
    inline Value Dict::get(slice key) const   {return FLDict_Get(*this, key);}
    inline Value Dict::get(Dict::Key &key) const{return FLDict_GetWithKey(*this, &key._key);}

    inline Dict::Key::Key(slice s)            :_key(FLDictKey_Init(s)) { }
    inline slice Dict::Key::string() const   {return FLDictKey_GetString(&_key);}

    inline Dict::iterator::iterator(Dict d)     {FLDictIterator_Begin(d, this);}
    inline Value Dict::iterator::key() const    {return FLDictIterator_GetKey(this);}
    inline slice Dict::iterator::keyString() const {return FLDictIterator_GetKeyString(this);}
    inline Value Dict::iterator::value() const  {return FLDictIterator_GetValue(this);}
    inline bool Dict::iterator::next()          {return FLDictIterator_Next(this);}

    inline void Encoder::amend(slice base, bool reuseStrings, bool externPointers)
                                                {FLEncoder_Amend(_enc, base,
                                                                     reuseStrings, externPointers);}
    inline bool Encoder::writeNull()            {return FLEncoder_WriteNull(_enc);}
    inline bool Encoder::writeUndefined()       {return FLEncoder_WriteUndefined(_enc);}
    inline bool Encoder::writeBool(bool b)      {return FLEncoder_WriteBool(_enc, b);}
    inline bool Encoder::writeInt(int64_t n)    {return FLEncoder_WriteInt(_enc, n);}
    inline bool Encoder::writeUInt(uint64_t n)  {return FLEncoder_WriteUInt(_enc, n);}
    inline bool Encoder::writeFloat(float n)    {return FLEncoder_WriteFloat(_enc, n);}
    inline bool Encoder::writeDouble(double n)  {return FLEncoder_WriteDouble(_enc, n);}
    inline bool Encoder::writeString(slice s)   {return FLEncoder_WriteString(_enc, s);}
    inline bool Encoder::writeDateString(FLTimestamp ts, bool asUTC)
                                                {return FLEncoder_WriteDateString(_enc, ts, asUTC);}
    inline bool Encoder::writeData(slice data){return FLEncoder_WriteData(_enc, data);}
    inline bool Encoder::writeValue(Value v)    {return FLEncoder_WriteValue(_enc, v);}
    inline bool Encoder::convertJSON(slice j) {return FLEncoder_ConvertJSON(_enc, j);}
    inline bool Encoder::beginArray(size_t rsv) {return FLEncoder_BeginArray(_enc, rsv);}
    inline bool Encoder::endArray()             {return FLEncoder_EndArray(_enc);}
    inline bool Encoder::beginDict(size_t rsv)  {return FLEncoder_BeginDict(_enc, rsv);}
    inline bool Encoder::writeKey(slice key)    {return FLEncoder_WriteKey(_enc, key);}
    inline bool Encoder::writeKey(Value key)    {return FLEncoder_WriteKeyValue(_enc, key);}
    inline bool Encoder::endDict()              {return FLEncoder_EndDict(_enc);}
    inline size_t Encoder::bytesWritten() const {return FLEncoder_BytesWritten(_enc);}
    inline Doc Encoder::finishDoc(FLError* err) {return FLEncoder_FinishDoc(_enc, err);}
    inline alloc_slice Encoder::finish(FLError* err) {return FLEncoder_Finish(_enc, err);}
    inline void Encoder::reset()                {return FLEncoder_Reset(_enc);}
    inline FLError Encoder::error() const       {return FLEncoder_GetError(_enc);}
    inline const char* Encoder::errorMessage() const {return FLEncoder_GetErrorMessage(_enc);}

    // specialization for assigning bool value since there is no Encoder<<bool
    template<>
    inline void Encoder::keyref::operator= (bool value) {_enc.writeKey(_key); _enc.writeBool(value);}

    inline fleece::alloc_slice JSONDelta::create(Value old, Value nuu) {
        return FLCreateJSONDelta(old, nuu);
    }
    inline bool JSONDelta::create(Value old, Value nuu, Encoder &jsonEncoder) {
        return FLEncodeJSONDelta(old, nuu, jsonEncoder);
    }
    inline fleece::alloc_slice JSONDelta::apply(Value old,
                                                fleece::slice jsonDelta,
                                                FLError *error) {
        return FLApplyJSONDelta(old, jsonDelta, error);
    }
    inline bool JSONDelta::apply(Value old,
                                 Value jsonDelta,
                                 Encoder &encoder) {
        return FLEncodeApplyingJSONDelta(old, jsonDelta, encoder);
    }

    inline Doc Doc::fromJSON(fleece::slice json, FLError *outError) {
        return Doc(FLDoc_FromJSON(json, outError), false);
    }

    inline Doc& Doc::operator=(const Doc &other) {
        if (other._doc != _doc) {
            FLDoc_Release(_doc);
            _doc = FLDoc_Retain(other._doc);
        }
        return *this;
    }

   inline Doc& Doc::operator=(Doc &&other) {
        if (other._doc != _doc) {
            FLDoc_Release(_doc);
            _doc = other._doc;
            other._doc = nullptr;
        }
        return *this;
    }

}

#endif // _FLEECE_HH
