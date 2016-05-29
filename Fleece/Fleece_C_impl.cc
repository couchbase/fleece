//
//  fleece_c.cc
//  Fleece
//
//  Created by Jens Alfke on 5/13/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "Fleece.hh"
using namespace fleece;

#define FL_IMPL
typedef const Value* FLValue;
typedef const Array* FLArray;
typedef const Dict* FLDict;
typedef Encoder* FLEncoder;
typedef slice FLSlice;

#include "fleece.h"


void FLSliceFree(FLSliceResult s)               {free(s.buf);}

FLValue FLValueFromData(FLSlice data)           {return Value::fromData(data);}
FLValue FLValueFromTrustedData(FLSlice data)    {return Value::fromTrustedData(data);}
FLValueType FLValueGetType(FLValue v)           {return (FLValueType)v->type();}
bool FLValueIsInteger(FLValue v)                {return v->isInteger();}
bool FLValueIsUnsigned(FLValue v)               {return v->isUnsigned();}
bool FLValueIsDouble(FLValue v)                 {return v->isDouble();}
bool FLValueAsBool(FLValue v)                   {return v->asBool();}
int64_t FLValueAsInt(FLValue v)                 {return v->asInt();}
uint64_t FLValueAsUnsigned(FLValue v)           {return v->asUnsigned();}
float FLValueAsFloat(FLValue v)                 {return v->asFloat();}
double FLValueAsDouble(FLValue v)               {return v->asDouble();}
FLSlice FLValueAsString(FLValue v)              {return v->asString();}
FLArray FLValueAsArray(FLValue v)               {return v->asArray();}
FLDict FLValueAsDict(FLValue v)                 {return v->asDict();}

static FLSliceResult strToSlice(std::string str) {
    FLSliceResult result;
    result.size = str.size();
    if (result.size == 0)
        return {NULL, 0};
    result.buf = malloc(result.size);
    if (!result.buf)
        return {NULL, 0};
    memcpy(result.buf, str.data(), result.size);
    return result;
}

FLSliceResult FLValueToString(FLValue v) {
    return strToSlice(v->toString());
}

FLSliceResult FLValueToJSON(FLValue v) {
    alloc_slice json = v->toJSON();
    json.dontFree();
    return {(void*)json.buf, json.size};
}

FLSliceResult FLDataDump(FLSlice data) {
    return strToSlice(Value::dump(data));
}

uint32_t FLArrayCount(FLArray a)                    {return a->count();}
FLValue FLArrayGet(FLArray a, uint32_t index)       {return a->get(index);}

void FLArrayIteratorBegin(FLArray a, FLArrayIterator* i) {
    static_assert(sizeof(FLArrayIterator) >= sizeof(Array::iterator),"FLArrayIterator is too small");
    new (i) Array::iterator(a);
}

FLValue FLArrayIteratorGetValue(const FLArrayIterator* i) {
    return ((Array::iterator*)i)->value();
}

bool FLArrayIteratorNext(FLArrayIterator* i) {
    auto& iter = *(Array::iterator*)i;
    ++iter;
    return (bool)iter;
}


uint32_t FLDictCount(FLDict d)                          {return d->count();}
FLValue FLDictGet(FLDict d, FLSlice keyString)          {return d->get(keyString);}
FLValue FLDictGetUnsorted(FLDict d, FLSlice keyString)  {return d->get_unsorted(keyString);}

void FLDictIteratorBegin(FLDict d, FLDictIterator* i) {
    static_assert(sizeof(FLDictIterator) >= sizeof(Dict::iterator), "FLDictIterator is too small");
    new (i) Dict::iterator(d);
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
    auto key = *(Dict::key*)k;
    return d->get(key);
}

size_t FLDictGetWithKeys(FLDict d, FLDictKey keys[], FLValue values[], size_t count) {
    return d->get((Dict::key*)keys, values, count);
}


FLEncoder FLEncoderNew(void)                            {return new Encoder();}

FLEncoder FLEncoderNewWithOptions(size_t reserveSize, bool uniqueStrings, bool sortKeys) {
    auto e = new Encoder(reserveSize);
    e->uniqueStrings(uniqueStrings);
    e->sortKeys(sortKeys);
    return e;
}

void FLEncoderReset(FLEncoder e)                        {e->reset();}
void FLEncoderFree(FLEncoder e)                         {delete e;}

void FLEncoderWriteNull(FLEncoder e)                    {e->writeNull();}
void FLEncoderWriteBool(FLEncoder e, bool b)            {e->writeBool(b);}
void FLEncoderWriteInt(FLEncoder e, int64_t i)          {e->writeInt(i);}
void FLEncoderWriteUInt(FLEncoder e, uint64_t u)        {e->writeUInt(u);}
void FLEncoderWriteFloat(FLEncoder e, float f)          {e->writeFloat(f);}
void FLEncoderWriteDouble(FLEncoder e, double d)        {e->writeDouble(d);}
void FLEncoderWriteString(FLEncoder e, FLSlice s)       {e->writeString(s);}
void FLEncoderWriteData(FLEncoder e, FLSlice d)         {e->writeData(d);}

void FLEncoderBeginArray(FLEncoder e, size_t reserve)   {e->beginArray(reserve);}
void FLEncoderEndArray(FLEncoder e)                     {e->endArray();}

void FLEncoderBeginDict(FLEncoder e, size_t reserve)    {e->beginDictionary(reserve);}
void FLEncoderWriteKey(FLEncoder e, FLSlice s)          {e->writeKey(s);}
void FLEncoderEndDict(FLEncoder e)                      {e->endDictionary();}

FLSliceResult FLEncoderEnd(FLEncoder e) {
    alloc_slice result = e->extractOutput();
    result.dontFree();
    return {(void*)result.buf, result.size};
}

FLSliceResult FLConvertJSON(FLSlice json, int *outError) {
    Encoder e(json.size);
    JSONConverter jc(e);
    if (!jc.convertJSON(json)) {
        if (outError) *outError = jc.error();
        return {NULL, 0};
    }
    return FLEncoderEnd(&e);
}
