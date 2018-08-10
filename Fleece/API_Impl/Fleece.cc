//
// Fleece.cc
//
// Copyright (c) 2016 Couchbase, Inc All rights reserved.
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

#include "Fleece+ImplGlue.hh"
#include "MutableArray.hh"
#include "MutableDict.hh"
#include "Delta.hh"
#include "fleece/Fleece.h"
#include "JSON5.hh"


namespace fleece { namespace impl {

    void recordError(const std::exception &x, FLError *outError) noexcept {
        if (outError)
            *outError = (FLError) FleeceException::getCode(x);
    }

} }


bool FLSlice_Equal(FLSlice a, FLSlice b)        {return (slice)a == (slice)b;}
int FLSlice_Compare(FLSlice a, FLSlice b)       {return ((slice)a).compare((slice)b); }


static FLSliceResult toSliceResult(alloc_slice &&s) {
    s.retain();
    return {(void*)s.buf, s.size};
}

void FLSliceResult_Free(FLSliceResult s) {
    alloc_slice::release({s.buf, s.size});
}


FLValue FLValue_FromData(FLSlice data, FLTrust trust)   {
    return trust ? Value::fromTrustedData(data) : Value::fromData(data);
}


const char* FLDump(FLValue v) {
    FLStringResult json = FLValue_ToJSON(v);
    auto cstr = (char*)malloc(json.size + 1);
    memcpy(cstr, json.buf, json.size);
    cstr[json.size] = 0;
    return cstr;
}

const char* FLDumpData(FLSlice data) {
    return FLDump(Value::fromData(data));
}



FLValueType FLValue_GetType(FLValue v)          {return v ? (FLValueType)v->type() : kFLUndefined;}
bool FLValue_IsInteger(FLValue v)               {return v && v->isInteger();}
bool FLValue_IsUnsigned(FLValue v)              {return v && v->isUnsigned();}
bool FLValue_IsDouble(FLValue v)                {return v && v->isDouble();}
bool FLValue_AsBool(FLValue v)                  {return v && v->asBool();}
int64_t FLValue_AsInt(FLValue v)                {return v ? v->asInt() : 0;}
uint64_t FLValue_AsUnsigned(FLValue v)          {return v ? v->asUnsigned() : 0;}
float FLValue_AsFloat(FLValue v)                {return v ? v->asFloat() : 0.0;}
double FLValue_AsDouble(FLValue v)              {return v ? v->asDouble() : 0.0;}
FLString FLValue_AsString(FLValue v)            {return v ? (FLString)v->asString() : kFLSliceNull;}
FLSlice FLValue_AsData(FLValue v)               {return v ? (FLSlice)v->asData() : kFLSliceNull;}
FLArray FLValue_AsArray(FLValue v)              {return v ? v->asArray() : nullptr;}
FLDict FLValue_AsDict(FLValue v)                {return v ? v->asDict() : nullptr;}


FLDoc FLValue_FindDoc(FLValue v) {
    return v ? retain(Doc::containing(v).get()) : nullptr;
}

FLSliceResult FLValue_ToString(FLValue v) {
    if (v) {
        try {
            return toSliceResult(v->toString());    // toString can throw
        } catchError(nullptr)
    }
    return {nullptr, 0};
}


FLSliceResult FLValue_ToJSONX(FLValue v,
                              bool json5,
                              bool canonical)
{
    if (v) {
        try {
            JSONEncoder encoder;
            encoder.setJSON5(json5);
            encoder.setCanonical(canonical);
            encoder.writeValue(v);
            return toSliceResult(encoder.finish());
        } catchError(nullptr)
    }
    return {nullptr, 0};
}

FLSliceResult FLValue_ToJSON(FLValue v)      {return FLValue_ToJSONX(v, false, false);}
FLSliceResult FLValue_ToJSON5(FLValue v)     {return FLValue_ToJSONX(v, true,  false);}


FLSliceResult FLData_ConvertJSON(FLSlice json, FLError *outError) {
    FLEncoderImpl e(kFLEncodeFleece, json.size);
    FLEncoder_ConvertJSON(&e, json);
    return FLEncoder_Finish(&e, outError);
}


FLSliceResult FLJSON5_ToJSON(FLSlice json5, FLError *error) {
    try {
        std::string json = ConvertJSON5((std::string((char*)json5.buf, json5.size)));
        return toSliceResult(alloc_slice(json));
    } catchError(nullptr)
    return {};
}


FLSliceResult FLData_Dump(FLSlice data) {
    try {
        return toSliceResult(alloc_slice(Value::dump(data)));
    } catchError(nullptr)
    return {nullptr, 0};
}


#pragma mark - ARRAYS:


uint32_t FLArray_Count(FLArray a)                    {return a ? a->count() : 0;}
bool FLArray_IsEmpty(FLArray a)                      {return a ? a->empty() : true;}
FLValue FLArray_Get(FLArray a, uint32_t index)       {return a ? a->get(index) : nullptr;}

void FLArrayIterator_Begin(FLArray a, FLArrayIterator* i) {
    static_assert(sizeof(FLArrayIterator) >= sizeof(Array::iterator),"FLArrayIterator is too small");
    new (i) Array::iterator(a);
    // Note: this is safe even if a is null.
}

uint32_t FLArrayIterator_GetCount(const FLArrayIterator* i) {
    return ((Array::iterator*)i)->count();
}

FLValue FLArrayIterator_GetValue(const FLArrayIterator* i) {
    return ((Array::iterator*)i)->value();
}

FLValue FLArrayIterator_GetValueAt(const FLArrayIterator *i, uint32_t offset) {
    return (*(Array::iterator*)i)[offset];
}

bool FLArrayIterator_Next(FLArrayIterator* i) {
    try {
        auto& iter = *(Array::iterator*)i;
        ++iter;                 // throws if iterating past end
        return (bool)iter;
    } catchError(nullptr)
    return false;
}


static FLMutableArray _newMutableArray(FLArray a) noexcept {
    try {
        return (MutableArray*)retain(MutableArray::newArray(a));
    } catchError(nullptr)
    return nullptr;
}

FLMutableArray FLArray_MutableCopy(FLArray a)       {return a ? _newMutableArray(a) : nullptr;}
FLMutableArray FLMutableArray_New(void)             {return FLArray_MutableCopy(nullptr);}
FLMutableArray FLArray_AsMutable(FLArray a)         {return a ? a->asMutable() : nullptr;}
FLMutableArray FLMutableArray_Retain(FLMutableArray a) {return retain(a);}
void FLMutableArray_Release(FLMutableArray a)       {release(a);}
FLArray FLMutableArray_GetSource(FLMutableArray a)  {return a ? a->source() : nullptr;}
bool FLMutableArray_IsChanged(FLMutableArray a)     {return a && a->isChanged();}
void FLMutableArray_Resize(FLMutableArray a, uint32_t size)     {a->resize(size);}

void FLMutableArray_AppendNull(FLMutableArray a)                {if(a) a->append(nullValue);}
void FLMutableArray_AppendBool(FLMutableArray a, bool v)        {if(a) a->append(v);}
void FLMutableArray_AppendInt(FLMutableArray a, int64_t v)      {if(a) a->append(v);}
void FLMutableArray_AppendUInt(FLMutableArray a, uint64_t v)    {if(a) a->append(v);}
void FLMutableArray_AppendFloat(FLMutableArray a, float v)      {if(a) a->append(v);}
void FLMutableArray_AppendDouble(FLMutableArray a, double v)    {if(a) a->append(v);}
void FLMutableArray_AppendString(FLMutableArray a, FLString v)  {if(a) a->append(v);}
void FLMutableArray_AppendData(FLMutableArray a, FLSlice v)     {if(a) a->append(v);}
void FLMutableArray_AppendValue(FLMutableArray a, FLValue v)    {if(a) a->append(v);}

void FLMutableArray_SetNull(FLMutableArray a, uint32_t i)               {if(a) a->set(i, nullValue);}
void FLMutableArray_SetBool(FLMutableArray a, uint32_t i, bool v)       {if(a) a->set(i, v);}
void FLMutableArray_SetInt(FLMutableArray a, uint32_t i, int64_t v)     {if(a) a->set(i, v);}
void FLMutableArray_SetUInt(FLMutableArray a, uint32_t i, uint64_t v)   {if(a) a->set(i, v);}
void FLMutableArray_SetFloat(FLMutableArray a, uint32_t i, float v)     {if(a) a->set(i, v);}
void FLMutableArray_SetDouble(FLMutableArray a, uint32_t i, double v)   {if(a) a->set(i, v);}
void FLMutableArray_SetString(FLMutableArray a, uint32_t i, FLString v) {if(a) a->set(i, v);}
void FLMutableArray_SetData(FLMutableArray a, uint32_t i, FLSlice v)    {if(a) a->set(i, v);}
void FLMutableArray_SetValue(FLMutableArray a, uint32_t i, FLValue v)   {if(a) a->set(i, v);}

void FLMutableArray_Remove(FLMutableArray a, uint32_t firstIndex, uint32_t count) {
    if(a) a->remove(firstIndex, count);
}

FLMutableArray FLMutableArray_GetMutableArray(FLMutableArray a, uint32_t index) {
    return a ? a->getMutableArray(index) : nullptr;
}

FLMutableDict FLMutableArray_GetMutableDict(FLMutableArray a, uint32_t index) {
    return a ? a->getMutableDict(index) : nullptr;
}


#pragma mark - DICTIONARIES:


uint32_t FLDict_Count(FLDict d)                          {return d ? d->count() : 0;}
bool FLDict_IsEmpty(FLDict d)                            {return d ? d->empty() : true;}
FLValue FLDict_Get(FLDict d, FLSlice keyString)          {return d ? d->get(keyString) : nullptr;}

#if 0
FLSlice FLSharedKey_GetKeyString(FLSharedKeys sk, int keyCode, FLError* outError)
{
    slice key;
    try {
        key = sk->decode((keyCode));
        if(!key && outError != nullptr) {
            *outError = kFLNotFound;
        }
    } catchError(outError)
    
    return key;
}
#endif

void FLDictIterator_Begin(FLDict d, FLDictIterator* i) {
    static_assert(sizeof(FLDictIterator) >= sizeof(Dict::iterator), "FLDictIterator is too small");
    new (i) Dict::iterator(d);
    // Note: this is safe even if d is null.
}

FLValue FLDictIterator_GetKey(const FLDictIterator* i) {
    return ((Dict::iterator*)i)->key();
}

FLString FLDictIterator_GetKeyString(const FLDictIterator* i) {
    return ((Dict::iterator*)i)->keyString();
}

FLValue FLDictIterator_GetValue(const FLDictIterator* i) {
    return ((Dict::iterator*)i)->value();
}

uint32_t FLDictIterator_GetCount(const FLDictIterator* i) {
    return ((Dict::iterator*)i)->count();
}

bool FLDictIterator_Next(FLDictIterator* i) {
    try {
        auto& iter = *(Dict::iterator*)i;
        ++iter;                 // throws if iterating past end
        if (iter)
            return true;
        iter.~iterator();
    } catchError(nullptr)
    return false;
}

void FLDictIterator_End(FLDictIterator* i) {
    ((Dict::iterator*)i)->~iterator();
}


FLDictKey FLDictKey_Init(FLSlice string) {
    FLDictKey key;
    static_assert(sizeof(FLDictKey) >= sizeof(Dict::key), "FLDictKey is too small");
    new (&key) Dict::key(string);
    return key;
}

FLSlice FLDictKey_GetString(const FLDictKey *key) {
    auto realKey = (const Dict::key*)key;
    return realKey->string();
}

FLValue FLDict_GetWithKey(FLDict d, FLDictKey *k) {
    if (!d)
        return nullptr;
    auto &key = *(Dict::key*)k;
    return d->get(key);
}


static FLMutableDict _newMutableDict(FLDict d) noexcept {
    try {
        return (MutableDict*)retain(MutableDict::newDict(d));
    } catchError(nullptr)
    return nullptr;
}

FLMutableDict FLDict_MutableCopy(FLDict d)          {return d ? _newMutableDict(d) : nullptr;}
FLMutableDict FLMutableDict_New(void)               {return FLDict_MutableCopy(nullptr);}
FLMutableDict FLDict_AsMutable(FLDict d)            {return d ? d->asMutable() : nullptr;}
FLMutableDict FLMutableDict_Retain(FLMutableDict d) {return retain(d);}
void FLMutableDict_Release(FLMutableDict d)         {release(d);}
FLDict FLMutableDict_GetSource(FLMutableDict d)     {return d ? d->source() : nullptr;}
bool FLMutableDict_IsChanged(FLMutableDict d)       {return d && d->isChanged();}

void FLMutableDict_SetNull(FLMutableDict d, FLString k)               {if(d) d->set(k, nullValue);}
void FLMutableDict_SetBool(FLMutableDict d, FLString k, bool v)         {if(d) d->set(k, v);}
void FLMutableDict_SetInt(FLMutableDict d, FLString k, int64_t v)       {if(d) d->set(k, v);}
void FLMutableDict_SetUInt(FLMutableDict d, FLString k, uint64_t v)     {if(d) d->set(k, v);}
void FLMutableDict_SetFloat(FLMutableDict d, FLString k, float v)       {if(d) d->set(k, v);}
void FLMutableDict_SetDouble(FLMutableDict d, FLString k, double v)     {if(d) d->set(k, v);}
void FLMutableDict_SetString(FLMutableDict d, FLString k, FLString v)   {if(d) d->set(k, v);}
void FLMutableDict_SetData(FLMutableDict d, FLString k, FLSlice v)      {if(d) d->set(k, v);}
void FLMutableDict_SetValue(FLMutableDict d, FLString k, FLValue v)     {if(d) d->set(k, v);}

void FLMutableDict_Remove(FLMutableDict d, FLString key)                {if(d) d->remove(key);}

FLMutableArray FLMutableDict_GetMutableArray(FLMutableDict d, FLString key) {
    return d ? d->getMutableArray(key) : nullptr;
}

FLMutableDict FLMutableDict_GetMutableDict(FLMutableDict d, FLString key) {
    return d ? d->getMutableDict(key) : nullptr;
}


//////// SHARED KEYS


FLSharedKeys FLSharedKeys_Create()                          {return retain(new SharedKeys());}
FLSharedKeys FLSharedKeys_Retain(FLSharedKeys sk)           {return retain(sk);}
void FLSharedKeys_Release(FLSharedKeys sk)                  {release(sk);}
FLSharedKeys FLSharedKeys_CreateFromStateData(FLSlice data) {return retain(new SharedKeys(data));}
FLSliceResult FLSharedKeys_GetStateData(FLSharedKeys sk)    {return toSliceResult(sk->stateData());}
FLString FLSharedKeys_Decode(FLSharedKeys sk, int key)      {return sk->decode(key);}

int FLSharedKeys_Encode(FLSharedKeys sk, FLString keyStr, bool add) {
    int intKey;
    if (!(add ? sk->encodeAndAdd(keyStr, intKey) : sk->encode(keyStr, intKey)))
        intKey = -1;
    return intKey;
}


#pragma mark - DEEP ITERATOR:


FLDeepIterator FLDeepIterator_New(FLValue v)                    {return new DeepIterator(v);}
void FLDeepIterator_Free(FLDeepIterator i)                      {delete i;}
FLValue FLDeepIterator_GetValue(FLDeepIterator i)               {return i->value();}
FLSlice FLDeepIterator_GetKey(FLDeepIterator i)                 {return i->keyString();}
uint32_t FLDeepIterator_GetIndex(FLDeepIterator i)              {return i->index();}
size_t FLDeepIterator_GetDepth(FLDeepIterator i)                {return i->path().size();}
void FLDeepIterator_SkipChildren(FLDeepIterator i)              {i->skipChildren();}

bool FLDeepIterator_Next(FLDeepIterator i) {
    i->next();
    return i->value() != nullptr;
}

void FLDeepIterator_GetPath(FLDeepIterator i, FLPathComponent* *outPath, size_t *outDepth) {
    static_assert(sizeof(FLPathComponent) == sizeof(DeepIterator::PathComponent),
                  "FLPathComponent does not match PathComponent");
    auto &path = i->path();
    *outPath = (FLPathComponent*) path.data();
    *outDepth = path.size();
}

FLSliceResult FLDeepIterator_GetPathString(FLDeepIterator i) {
    return toSliceResult(alloc_slice(i->pathString()));
}

FLSliceResult FLDeepIterator_GetJSONPointer(FLDeepIterator i) {
    return toSliceResult(alloc_slice(i->jsonPointer()));
}


#pragma mark - KEY-PATHS:


FLKeyPath FLKeyPath_New(FLSlice specifier, FLError *outError) {
    try {
        return new Path((std::string)(slice)specifier);
    } catchError(outError)
    return nullptr;
}

void FLKeyPath_Free(FLKeyPath path) {
    delete path;
}

FLValue FLKeyPath_Eval(FLKeyPath path, FLValue root) {
    return path->eval(root);
}

FLValue FLKeyPath_EvalOnce(FLSlice specifier, FLValue root, FLError *outError)
{
    try {
        return Path::eval((std::string)(slice)specifier, root);
    } catchError(outError)
    return nullptr;
}


#pragma mark - ENCODER:


FLEncoder FLEncoder_New(void) {
    return FLEncoder_NewWithOptions(kFLEncodeFleece, 0, true);
}

FLEncoder FLEncoder_NewWithOptions(FLEncoderFormat format,
                                   size_t reserveSize, bool uniqueStrings)
{
    return new FLEncoderImpl(format, reserveSize, uniqueStrings);
}

void FLEncoder_Reset(FLEncoder e) {
    e->reset();
}

void FLEncoder_Free(FLEncoder e)                         {
    delete e;
}

void FLEncoder_SetSharedKeys(FLEncoder e, FLSharedKeys sk) {
    if (e->isFleece())
        e->fleeceEncoder->setSharedKeys(sk);
}

void FLEncoder_MakeDelta(FLEncoder e, FLSlice base, bool reuseStrings, bool externPointers) {
    if (e->isFleece()) {
        e->fleeceEncoder->setBase(base, externPointers);
        if(reuseStrings)
            e->fleeceEncoder->reuseBaseStrings();
    }
}

size_t FLEncoder_BytesWritten(FLEncoder e) {
    return ENCODER_DO(e, bytesWritten());
}

bool FLEncoder_WriteNull(FLEncoder e)                    {ENCODER_TRY(e, writeNull());}
bool FLEncoder_WriteBool(FLEncoder e, bool b)            {ENCODER_TRY(e, writeBool(b));}
bool FLEncoder_WriteInt(FLEncoder e, int64_t i)          {ENCODER_TRY(e, writeInt(i));}
bool FLEncoder_WriteUInt(FLEncoder e, uint64_t u)        {ENCODER_TRY(e, writeUInt(u));}
bool FLEncoder_WriteFloat(FLEncoder e, float f)          {ENCODER_TRY(e, writeFloat(f));}
bool FLEncoder_WriteDouble(FLEncoder e, double d)        {ENCODER_TRY(e, writeDouble(d));}
bool FLEncoder_WriteString(FLEncoder e, FLSlice s)       {ENCODER_TRY(e, writeString(s));}
bool FLEncoder_WriteData(FLEncoder e, FLSlice d)         {ENCODER_TRY(e, writeData(d));}
bool FLEncoder_WriteRaw(FLEncoder e, FLSlice r)          {ENCODER_TRY(e, writeRaw(r));}
bool FLEncoder_WriteValue(FLEncoder e, FLValue v)        {ENCODER_TRY(e, writeValue(v));}

bool FLEncoder_BeginArray(FLEncoder e, size_t reserve)   {ENCODER_TRY(e, beginArray(reserve));}
bool FLEncoder_EndArray(FLEncoder e)                     {ENCODER_TRY(e, endArray());}
bool FLEncoder_BeginDict(FLEncoder e, size_t reserve)    {ENCODER_TRY(e, beginDictionary(reserve));}
bool FLEncoder_WriteKey(FLEncoder e, FLSlice s)          {ENCODER_TRY(e, writeKey(s));}
bool FLEncoder_EndDict(FLEncoder e)                      {ENCODER_TRY(e, endDictionary());}


bool FLEncoder_ConvertJSON(FLEncoder e, FLSlice json) {
    if (!e->hasError()) {
        try {
            if (e->isFleece()) {
                JSONConverter *jc = e->jsonConverter.get();
                if (jc) {
                    jc->reset();
                } else {
                    jc = new JSONConverter(*e->fleeceEncoder);
                    e->jsonConverter.reset(jc);
                }
                if (jc->encodeJSON(json)) {                   // encodeJSON can throw
                    return true;
                } else {
                    e->errorCode = (FLError)jc->errorCode();
                    e->errorMessage = jc->errorMessage();
                }
            } else {
                e->jsonEncoder->writeJSON(json);
            }
        } catch (const std::exception &x) {
            e->recordException(x);
        }
    }
    return false;
}

FLError FLEncoder_GetError(FLEncoder e) {
    return (FLError)e->errorCode;
}

const char* FLEncoder_GetErrorMessage(FLEncoder e) {
    return e->hasError() ? e->errorMessage.c_str() : nullptr;
}

void FLEncoder_SetExtraInfo(FLEncoder e, void *info) {
    e->extraInfo = info;
}

void* FLEncoder_GetExtraInfo(FLEncoder e) {
    return e->extraInfo;
}

FLDoc FLEncoder_FinishDoc(FLEncoder e, FLError *outError) {
    if (!e->fleeceEncoder)
        e->errorCode = kFLUnsupported;  // Doc class doesn't support JSON data
    if (!e->hasError()) {
        try {
            return retain(e->fleeceEncoder->finishDoc().get());       // finish() can throw
        } catch (const std::exception &x) {
            e->recordException(x);
        }
    }
    // Failure:
    if (outError)
        *outError = e->errorCode;
    return nullptr;
}


FLSliceResult FLEncoder_Finish(FLEncoder e, FLError *outError) {
    if (!e->hasError()) {
        try {
            return toSliceResult(ENCODER_DO(e, finish()));       // extractOutput can throw
        } catch (const std::exception &x) {
            e->recordException(x);
        }
    }
    // Failure:
    if (outError)
        *outError = e->errorCode;
    return {nullptr, 0};
}


#pragma mark - DOCUMENTS


FLDoc FLDoc_FromData(FLSlice data, FLTrust trust, FLSharedKeys sk, FLSlice externData) {
    return retain(new Doc(data, (Doc::Trust)trust, sk, externData));
}

FLDoc FLDoc_FromResultData(FLSliceResult data, FLTrust trust, FLSharedKeys sk, FLSlice externData) {
    return retain(new Doc(alloc_slice(data), (Doc::Trust)trust, sk, externData));
}

FLDoc FLDoc_FromJSON(FLSlice json, FLError *outError) {
    try {
        return retain(Doc::fromJSON(json).get());
    } catchError(outError);
    return nullptr;
}

void FLDoc_Release(FLDoc doc)                           {release(doc);}
FLDoc FLDoc_Retain(FLDoc doc)                           {return retain(doc);}

FLSharedKeys FLDoc_GetSharedKeys(FLDoc doc)             {return doc ? doc->sharedKeys() : nullptr;}
FLValue FLDoc_GetRoot(FLDoc doc)                        {return doc ? doc->root() : nullptr;}
FLSlice FLDoc_GetData(FLDoc doc)                        {return doc ? doc->data() : nullslice;}

FLSliceResult FLDoc_GetAllocedData(FLDoc doc) {
    return doc ? toSliceResult(doc->allocedData()) : FLSliceResult{};
}


#pragma mark - DELTA COMPRESSION


FLSliceResult FLCreateDelta(FLValue old, FLValue nuu) {
    return toSliceResult(Delta::create(old, nuu));
}

bool FLEncodeDelta(FLValue old, FLValue nuu, FLEncoder jsonEncoder) {
    JSONEncoder *enc = jsonEncoder->jsonEncoder.get();
    assert(enc);  //TODO: Support encoding to Fleece
    return Delta::create(old, nuu, *enc);
}


FLSliceResult FLApplyDelta(FLValue old, FLSlice jsonDelta, FLError *outError) {
    try {
        return toSliceResult(Delta::apply(old, jsonDelta));
    } catchError(outError);
    return {};
}

bool FLEncodeApplyingDelta(FLValue old, FLValue delta, FLEncoder encoder) {
    try {
        Encoder *enc = encoder->fleeceEncoder.get();
        if (!enc)
            FleeceException::_throw(EncodeError, "FLEncodeApplyingDelta cannot encode JSON");
        Delta::apply(old, delta, *enc);
        return true;
    } catch (const std::exception &x) {
        encoder->recordException(x);
        return false;
    }
}
