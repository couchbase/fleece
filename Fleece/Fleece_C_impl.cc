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


void FLSliceFree(FLSliceResult s)               {free(s.buf);}


FLValue FLValueFromData(FLSlice data, FLError *outError) {
    try {
        auto val = Value::fromData(data);
        if (val)
            return val;
        else if (outError)
            *outError = ::UnknownValue;
    } catchError(outError)
    return nullptr;
}


FLValue FLValueFromTrustedData(FLSlice data, FLError *outError) {
    try {
        auto val = Value::fromTrustedData(data);
        if (val)
            return val;
        else if (outError)
            *outError = ::UnknownValue;
    } catchError(outError)
    return nullptr;
}


FLSliceResult FLConvertJSON(FLSlice json, FLError *outError) {
    FLEncoderImpl e(json.size);
    try {
        JSONConverter jc(e);
        if (jc.convertJSON(json))
            return FLEncoderFinish(&e, outError);
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


FLValueType FLValueGetType(FLValue v)       {return v ? (FLValueType)v->type() : kFLUndefined;}
bool FLValueIsInteger(FLValue v)            {return v && v->isInteger();}
bool FLValueIsUnsigned(FLValue v)           {return v && v->isUnsigned();}
bool FLValueIsDouble(FLValue v)             {return v && v->isDouble();}
bool FLValueAsBool(FLValue v)               {return v && v->asBool();}
int64_t FLValueAsInt(FLValue v)             {return v ? v->asInt() : 0;}
uint64_t FLValueAsUnsigned(FLValue v)       {return v ? v->asUnsigned() : 0;}
float FLValueAsFloat(FLValue v)             {return v ? v->asFloat() : 0.0;}
double FLValueAsDouble(FLValue v)           {return v ? v->asDouble() : 0.0;}
FLSlice FLValueAsString(FLValue v)          {return v ? v->asString() : slice::null;}
FLArray FLValueAsArray(FLValue v)           {return v ? v->asArray() : nullptr;}
FLDict FLValueAsDict(FLValue v)             {return v ? v->asDict() : nullptr;}


static FLSliceResult strToSlice(std::string str) {
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


FLSliceResult FLValueToString(FLValue v) {
    if (v) {
        try {
            return strToSlice(v->toString());
        } catchError(nullptr)
    }
    return {nullptr, 0};
}

FLSliceResult FLValueToJSON(FLValue v) {
    if (v) {
        try {
            alloc_slice json = v->toJSON();
            json.dontFree();
            return {(void*)json.buf, json.size};
        } catchError(nullptr)
    }
    return {nullptr, 0};
}

FLSliceResult FLDataDump(FLSlice data) {
    try {
        return strToSlice(Value::dump(data));
    } catchError(nullptr)
    return {nullptr, 0};
}


#pragma mark - ARRAYS:


uint32_t FLArrayCount(FLArray a)                    {return a ? a->count() : 0;}
FLValue FLArrayGet(FLArray a, uint32_t index)       {return a ? a->get(index) : nullptr;}

void FLArrayIteratorBegin(FLArray a, FLArrayIterator* i) {
    static_assert(sizeof(FLArrayIterator) >= sizeof(Array::iterator),"FLArrayIterator is too small");
    new (i) Array::iterator(a);
    // Note: this is safe even if a is null.
}

FLValue FLArrayIteratorGetValue(const FLArrayIterator* i) {
    return ((Array::iterator*)i)->value();
}

bool FLArrayIteratorNext(FLArrayIterator* i) {
    auto& iter = *(Array::iterator*)i;
    ++iter;
    return (bool)iter;
}


#pragma mark - DICTIONARIES:


uint32_t FLDictCount(FLDict d)                          {return d ? d->count() : 0;}
FLValue FLDictGet(FLDict d, FLSlice keyString)          {return d ? d->get(keyString) : nullptr;}
FLValue FLDictGetUnsorted(FLDict d, FLSlice keyString)  {return d ? d->get_unsorted(keyString) : nullptr;}

void FLDictIteratorBegin(FLDict d, FLDictIterator* i) {
    static_assert(sizeof(FLDictIterator) >= sizeof(Dict::iterator), "FLDictIterator is too small");
    new (i) Dict::iterator(d);
    // Note: this is safe even if a is null.
}

FLValue FLDictIteratorGetKey(const FLDictIterator* i) {
    return ((Dict::iterator*)i)->key();
}
FLValue FLDictIteratorGetValue(const FLDictIterator* i) {
    return ((Dict::iterator*)i)->value();
}
bool FLDictIteratorNext(FLDictIterator* i) {
    auto& iter = *(Dict::iterator*)i;
    ++iter;
    return (bool)iter;
}


void FLDictKeyInit(FLDictKey* key, FLSlice string, bool cachePointers) {
    static_assert(sizeof(FLDictKey) >= sizeof(Dict::key), "FLDictKey is too small");
    new (key) Dict::key(string, cachePointers);
}

FLValue FLDictGetWithKey(FLDict d, FLDictKey *k) {
    if (!d)
        return nullptr;
    auto key = *(Dict::key*)k;
    return d->get(key);
}

size_t FLDictGetWithKeys(FLDict d, FLDictKey keys[], FLValue values[], size_t count) {
    return d->get((Dict::key*)keys, values, count);
}


#pragma mark - ENCODER:


FLEncoder FLEncoderNew(void) {
    return new FLEncoderImpl;
}

FLEncoder FLEncoderNewWithOptions(size_t reserveSize, bool uniqueStrings, bool sortKeys) {
    auto e = new FLEncoderImpl(reserveSize);
    e->uniqueStrings(uniqueStrings);
    e->sortKeys(sortKeys);
    return e;
}

void FLEncoderReset(FLEncoder e) {
    e->reset();
}

void FLEncoderFree(FLEncoder e)                         {
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


bool FLEncoderWriteNull(FLEncoder e)                    {ENCODER_TRY(writeNull());}
bool FLEncoderWriteBool(FLEncoder e, bool b)            {ENCODER_TRY(writeBool(b));}
bool FLEncoderWriteInt(FLEncoder e, int64_t i)          {ENCODER_TRY(writeNull());}
bool FLEncoderWriteUInt(FLEncoder e, uint64_t u)        {ENCODER_TRY(writeUInt(u));}
bool FLEncoderWriteFloat(FLEncoder e, float f)          {ENCODER_TRY(writeFloat(f));}
bool FLEncoderWriteDouble(FLEncoder e, double d)        {ENCODER_TRY(writeDouble(d));}
bool FLEncoderWriteString(FLEncoder e, FLSlice s)       {ENCODER_TRY(writeString(s));}
bool FLEncoderWriteData(FLEncoder e, FLSlice d)         {ENCODER_TRY(writeData(d));}

bool FLEncoderBeginArray(FLEncoder e, size_t reserve)   {ENCODER_TRY(beginArray(reserve));}
bool FLEncoderEndArray(FLEncoder e)                     {ENCODER_TRY(endArray());}
bool FLEncoderBeginDict(FLEncoder e, size_t reserve)    {ENCODER_TRY(beginDictionary(reserve));}
bool FLEncoderWriteKey(FLEncoder e, FLSlice s)          {ENCODER_TRY(writeKey(s));}
bool FLEncoderEndDict(FLEncoder e)                      {ENCODER_TRY(endDictionary());}


FLError FLEncoderGetError(FLEncoder e) {
    return (FLError)e->errorCode;
}

const char* FLEncoderGetErrorMessage(FLEncoder e) {
    return e->hasError() ? e->errorMessage.c_str() : nullptr;
}

FLSliceResult FLEncoderFinish(FLEncoder e, FLError *outError) {
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
