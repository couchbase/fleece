//
//  fleece_C_impl.cc
//  Fleece
//
//  Created by Jens Alfke on 5/13/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "Fleece.hh"
#include "FleeceException.hh"
using namespace fleece;

namespace fleece {
    struct FLEncoderImpl;
}

#define FL_IMPL
typedef const Value* FLValue;
typedef const Array* FLArray;
typedef const Dict* FLDict;
typedef slice FLSlice;
typedef FLEncoderImpl* FLEncoder;


#include "Fleece.h" /* the C header */


static void recordError(const std::exception &x, FLError *outError) {
    if (outError) {
        auto fleecex = dynamic_cast<const FleeceException*>(&x);
        if (fleecex)
            *outError = (FLError)fleecex->code;
        else if (nullptr != dynamic_cast<const std::bad_alloc*>(&x))
            *outError = ::MemoryError;
        else
            *outError = ::InternalError;
    }
}

#define catchError(OUTERROR) \
    catch (const std::exception &x) { recordError(x, OUTERROR); }


namespace fleece {

    // Implementation of FLEncoder: a subclass of Encoder that keeps track of its error state.
    struct FLEncoderImpl : public Encoder {
        FLError errorCode {::NoError};
        std::string errorMessage;

        FLEncoderImpl(size_t reserveOutputSize =256) :Encoder(reserveOutputSize) { }

        bool hasError() const {
            return errorCode != ::NoError;
        }

        void recordError(const std::exception &x) {
            if (!hasError()) {
                ::recordError(x, &errorCode);
                errorMessage = x.what();
            }
        }

        void reset() {              // careful, not a real override (non-virtual method)
            Encoder::reset();
            errorCode = ::NoError;
        }
    };

}


int FLSlice_Compare(FLSlice a, FLSlice b)       {return a.compare(b);}

void FLSlice_Free(FLSliceResult s)              {free(s.buf);}


FLValue FLValue_FromData(FLSlice data, FLError *outError) {
    try {
        auto val = Value::fromData(data);
        if (val)
            return val;
        else if (outError)
            *outError = ::UnknownValue;
    } catchError(outError)
    return nullptr;
}


FLValue FLValue_FromTrustedData(FLSlice data, FLError *outError) {
    try {
        auto val = Value::fromTrustedData(data);
        if (val)
            return val;
        else if (outError)
            *outError = ::UnknownValue;
    } catchError(outError)
    return nullptr;
}


FLSliceResult FLData_ConvertJSON(FLSlice json, FLError *outError) {
    FLEncoderImpl e(json.size);
    try {
        JSONConverter jc(e);
        if (jc.convertJSON(json))
            return FLEncoder_Finish(&e, outError);
        else {
            e.errorCode = ::JSONError; //TODO: Save value of jc.error() somewhere
            e.errorMessage = jc.errorMessage();
        }
    } catch (const std::exception &x) {
        e.recordError(x);
    }
    // Failure:
    if (outError)
        *outError = e.errorCode;
    return {nullptr, 0};
}


FLValueType FLValue_GetType(FLValue v)       {return v ? (FLValueType)v->type() : kFLUndefined;}
bool FLValue_IsInteger(FLValue v)            {return v && v->isInteger();}
bool FLValue_IsUnsigned(FLValue v)           {return v && v->isUnsigned();}
bool FLValue_IsDouble(FLValue v)             {return v && v->isDouble();}
bool FLValue_AsBool(FLValue v)               {return v && v->asBool();}
int64_t FLValue_AsInt(FLValue v)             {return v ? v->asInt() : 0;}
uint64_t FLValue_AsUnsigned(FLValue v)       {return v ? v->asUnsigned() : 0;}
float FLValue_AsFloat(FLValue v)             {return v ? v->asFloat() : 0.0;}
double FLValue_AsDouble(FLValue v)           {return v ? v->asDouble() : 0.0;}
FLSlice FLValue_AsString(FLValue v)          {return v ? v->asString() : slice::null;}
FLArray FLValue_AsArray(FLValue v)           {return v ? v->asArray() : nullptr;}
FLDict FLValue_AsDict(FLValue v)             {return v ? v->asDict() : nullptr;}


static FLSliceResult toSliceResult(const std::string &str) {
    FLSliceResult result;
    result.size = str.size();
    if (result.size > 0) {
        result.buf = malloc(result.size);
        if (result.buf) {
            memcpy(result.buf, str.data(), result.size);
            return result;
        }
    }
    return {nullptr, 0};
}

static FLSliceResult toSliceResult(alloc_slice &&s) {
    s.dontFree();
    return {(void*)s.buf, s.size};
}


FLSliceResult FLValue_ToString(FLValue v) {
    if (v) {
        try {
            return toSliceResult(v->toString());
        } catchError(nullptr)
    }
    return {nullptr, 0};
}

FLSliceResult FLValue_ToJSON(FLValue v) {
    if (v) {
        try {
            return toSliceResult(v->toJSON());
        } catchError(nullptr)
    }
    return {nullptr, 0};
}

FLSliceResult FLData_Dump(FLSlice data) {
    try {
        return toSliceResult(Value::dump(data));
    } catchError(nullptr)
    return {nullptr, 0};
}


#pragma mark - ARRAYS:


uint32_t FLArray_Count(FLArray a)                    {return a ? a->count() : 0;}
FLValue FLArray_Get(FLArray a, uint32_t index)       {return a ? a->get(index) : nullptr;}

void FLArrayIterator_Begin(FLArray a, FLArrayIterator* i) {
    static_assert(sizeof(FLArrayIterator) >= sizeof(Array::iterator),"FLArrayIterator is too small");
    new (i) Array::iterator(a);
    // Note: this is safe even if a is null.
}

FLValue FLArrayIterator_GetValue(const FLArrayIterator* i) {
    return ((Array::iterator*)i)->value();
}

bool FLArrayIterator_Next(FLArrayIterator* i) {
    auto& iter = *(Array::iterator*)i;
    ++iter;
    return (bool)iter;
}


#pragma mark - DICTIONARIES:


uint32_t FLDict_Count(FLDict d)                          {return d ? d->count() : 0;}
FLValue FLDict_Get(FLDict d, FLSlice keyString)          {return d ? d->get(keyString) : nullptr;}
FLValue FLDict_GetUnsorted(FLDict d, FLSlice keyString)  {return d ? d->get_unsorted(keyString) : nullptr;}

void FLDictIterator_Begin(FLDict d, FLDictIterator* i) {
    static_assert(sizeof(FLDictIterator) >= sizeof(Dict::iterator), "FLDictIterator is too small");
    new (i) Dict::iterator(d);
    // Note: this is safe even if a is null.
}

FLValue FLDictIterator_GetKey(const FLDictIterator* i) {
    return ((Dict::iterator*)i)->key();
}
FLValue FLDictIterator_GetValue(const FLDictIterator* i) {
    return ((Dict::iterator*)i)->value();
}
bool FLDictIterator_Next(FLDictIterator* i) {
    auto& iter = *(Dict::iterator*)i;
    ++iter;
    return (bool)iter;
}


void FLDictKey_Init(FLDictKey* key, FLSlice string, bool cachePointers) {
    static_assert(sizeof(FLDictKey) >= sizeof(Dict::key), "FLDictKey is too small");
    new (key) Dict::key(string, cachePointers);
}

FLValue FLDict_GetWithKey(FLDict d, FLDictKey *k) {
    if (!d)
        return nullptr;
    auto key = *(Dict::key*)k;
    return d->get(key);
}

size_t FLDict_GetWithKeys(FLDict d, FLDictKey keys[], FLValue values[], size_t count) {
    return d->get((Dict::key*)keys, values, count);
}


#pragma mark - ENCODER:


FLEncoder FLEncoder_New(void) {
    return new FLEncoderImpl;
}

FLEncoder FLEncoder_NewWithOptions(size_t reserveSize, bool uniqueStrings, bool sortKeys) {
    auto e = new FLEncoderImpl(reserveSize);
    e->uniqueStrings(uniqueStrings);
    e->sortKeys(sortKeys);
    return e;
}

void FLEncoder_Reset(FLEncoder e) {
    e->reset();
}

void FLEncoder_Free(FLEncoder e)                         {
    delete e;
}


#define ENCODER_TRY(METHOD) \
    try{ \
        if (!e->hasError()) { \
            e->METHOD; \
            return true; \
        } \
    } catch (const std::exception &x) { \
        e->recordError(x); \
    } \
    return false;


bool FLEncoder_WriteNull(FLEncoder e)                    {ENCODER_TRY(writeNull());}
bool FLEncoder_WriteBool(FLEncoder e, bool b)            {ENCODER_TRY(writeBool(b));}
bool FLEncoder_WriteInt(FLEncoder e, int64_t i)          {ENCODER_TRY(writeNull());}
bool FLEncoder_WriteUInt(FLEncoder e, uint64_t u)        {ENCODER_TRY(writeUInt(u));}
bool FLEncoder_WriteFloat(FLEncoder e, float f)          {ENCODER_TRY(writeFloat(f));}
bool FLEncoder_WriteDouble(FLEncoder e, double d)        {ENCODER_TRY(writeDouble(d));}
bool FLEncoder_WriteString(FLEncoder e, FLSlice s)       {ENCODER_TRY(writeString(s));}
bool FLEncoder_WriteData(FLEncoder e, FLSlice d)         {ENCODER_TRY(writeData(d));}

bool FLEncoder_BeginArray(FLEncoder e, size_t reserve)   {ENCODER_TRY(beginArray(reserve));}
bool FLEncoder_EndArray(FLEncoder e)                     {ENCODER_TRY(endArray());}
bool FLEncoder_BeginDict(FLEncoder e, size_t reserve)    {ENCODER_TRY(beginDictionary(reserve));}
bool FLEncoder_WriteKey(FLEncoder e, FLSlice s)          {ENCODER_TRY(writeKey(s));}
bool FLEncoder_EndDict(FLEncoder e)                      {ENCODER_TRY(endDictionary());}


FLError FLEncoder_GetError(FLEncoder e) {
    return (FLError)e->errorCode;
}

const char* FLEncoder_GetErrorMessage(FLEncoder e) {
    return e->hasError() ? e->errorMessage.c_str() : nullptr;
}

FLSliceResult FLEncoder_Finish(FLEncoder e, FLError *outError) {
    if (!e->hasError()) {
        try {
            alloc_slice result = e->extractOutput();
            result.dontFree();
            return {(void*)result.buf, result.size};
        } catch (const std::exception &x) {
            e->recordError(x);
        }
    }
    // Failure:
    if (outError)
        *outError = e->errorCode;
    return {nullptr, 0};
}
