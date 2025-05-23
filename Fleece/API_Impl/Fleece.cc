//
// Fleece.cc
//
// Copyright 2016-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "Fleece+ImplGlue.hh"
#include "MutableArray.hh"
#include "MutableDict.hh"
#include "JSONDelta.hh"
#include "fleece/Fleece.h"
#include "JSON5.hh"
#include "ParseDate.hh"
#include "Builder.hh"
#include "betterassert.hh"
#include <chrono>


FL_ASSUME_NONNULL_BEGIN

namespace fleece::impl {

    void recordError(const std::exception &x, FLError* FL_NULLABLE outError) noexcept {
        if (outError)
            *outError = (FLError) FleeceException::getCode(x);
    }

}

using namespace fleece;
using namespace fleece::impl;


FLEECE_PUBLIC_IMPL const FLValue kFLNullValue  = Value::kNullValue;
FLEECE_PUBLIC_IMPL const FLValue kFLUndefinedValue  = Value::kUndefinedValue;
FLEECE_PUBLIC_IMPL const FLArray kFLEmptyArray = Array::kEmpty;
FLEECE_PUBLIC_IMPL const FLDict kFLEmptyDict   = Dict::kEmpty;


FLTimestamp FLTimestamp_Now() FLAPI {
    return FLTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch()).count());
}


FLStringResult FLTimestamp_ToString(FLTimestamp timestamp, bool asUTC) FLAPI {
    char str[kFormattedISO8601DateMaxSize];
    return FLSlice_Copy(FormatISO8601Date(str, timestamp, asUTC, nullptr));
}


FLTimestamp FLTimestamp_FromString(FLString str) FLAPI {
    return ParseISO8601Date(str);
}


FLValue FL_NULLABLE FLValue_FromData(FLSlice data, FLTrust trust) FLAPI {
    return trust ? Value::fromTrustedData(data) : Value::fromData(data);
}


const char* FL_NULLABLE FLDump(FLValue FL_NULLABLE v) FLAPI {
    FLStringResult json = FLValue_ToJSON(v);
    auto cstr = (char*)malloc(json.size + 1);
    FLMemCpy(cstr, json.buf, json.size);
    cstr[json.size] = 0;
    return cstr;
}

const char* FL_NULLABLE FLDumpData(FLSlice data) FLAPI {
    return FLDump(Value::fromData(data));
}


FLValueType FLValue_GetType(FLValue FL_NULLABLE v) FLAPI {
    if (_usuallyFalse(v == NULL))
        return kFLUndefined;
    auto type = (FLValueType)v->type();
    if (_usuallyFalse(type == kFLNull) && v->isUndefined())
        type = kFLUndefined;
    return type;
}


bool FLValue_IsInteger(FLValue FL_NULLABLE v)          FLAPI {return v && v->isInteger();}
bool FLValue_IsUnsigned(FLValue FL_NULLABLE v)         FLAPI {return v && v->isUnsigned();}
bool FLValue_IsDouble(FLValue FL_NULLABLE v)           FLAPI {return v && v->isDouble();}
bool FLValue_AsBool(FLValue FL_NULLABLE v)             FLAPI {return v && v->asBool();}
int64_t FLValue_AsInt(FLValue FL_NULLABLE v)           FLAPI {return v ? v->asInt() : 0;}
uint64_t FLValue_AsUnsigned(FLValue FL_NULLABLE v)     FLAPI {return v ? v->asUnsigned() : 0;}
float FLValue_AsFloat(FLValue FL_NULLABLE v)           FLAPI {return v ? v->asFloat() : 0.0f;}
double FLValue_AsDouble(FLValue FL_NULLABLE v)         FLAPI {return v ? v->asDouble() : 0.0;}
FLString FLValue_AsString(FLValue FL_NULLABLE v)       FLAPI {return v ? (FLString)v->asString() : kFLSliceNull;}
FLSlice FLValue_AsData(FLValue FL_NULLABLE v)          FLAPI {return v ? (FLSlice)v->asData() : kFLSliceNull;}
FLArray FL_NULLABLE FLValue_AsArray(FLValue FL_NULLABLE v)         FLAPI {return v ? v->asArray() : nullptr;}
FLDict FL_NULLABLE FLValue_AsDict(FLValue FL_NULLABLE v)           FLAPI {return v ? v->asDict() : nullptr;}
FLTimestamp FLValue_AsTimestamp(FLValue FL_NULLABLE v) FLAPI {return v ? v->asTimestamp() : FLTimestampNone;}
FLValue FL_NULLABLE FLValue_Retain(FLValue FL_NULLABLE v)          FLAPI {return retain(v);}
void FLValue_Release(FLValue FL_NULLABLE v)            FLAPI {release(v);}
bool FLValue_IsMutable(FLValue FL_NULLABLE v)          FLAPI {return v && v->isMutable();}


FLDoc FL_NULLABLE FLValue_FindDoc(FLValue FL_NULLABLE v) FLAPI {
    return v ? retain(Doc::containing(v).get()) : nullptr;
}

bool FLValue_IsEqual(FLValue FL_NULLABLE v1, FLValue FL_NULLABLE v2) FLAPI {
    if (_usuallyTrue(v1 != nullptr))
        return v1->isEqual(v2);
    else
        return v2 == nullptr;
}

FLSliceResult FLValue_ToString(FLValue FL_NULLABLE v) FLAPI {
    if (v) {
        try {
            return FLSliceResult(v->toString());    // toString can throw
        } catchError(nullptr)
    }
    return {nullptr, 0};
}


FLValue FL_NULLABLE FLValue_NewString(FLString str) FLAPI {
    try {
        return retain(internal::HeapValue::create(str))->asValue();
    } catchError(nullptr)
    return nullptr;
}


FLValue FL_NULLABLE FLValue_NewData(FLSlice data) FLAPI {
    try {
        return retain(internal::HeapValue::createData(data))->asValue();
    } catchError(nullptr)
    return nullptr;
}


FLSliceResult FLValue_ToJSONX(FLValue FL_NULLABLE v,
                              bool json5,
                              bool canonical) FLAPI
{
    if (v) {
        try {
            JSONEncoder encoder;
            encoder.setJSON5(json5);
            encoder.setCanonical(canonical);
            encoder.writeValue(v);
            return FLSliceResult(encoder.finish());
        } catchError(nullptr)
    }
    return {nullptr, 0};
}

FLSliceResult FLValue_ToJSON(FLValue FL_NULLABLE v)      FLAPI {return FLValue_ToJSONX(v, false, false);}
FLSliceResult FLValue_ToJSON5(FLValue FL_NULLABLE v)     FLAPI {return FLValue_ToJSONX(v, true,  false);}


FLSliceResult FLData_ConvertJSON(FLSlice json, FLError* FL_NULLABLE outError) FLAPI {
    FLEncoderImpl e(kFLEncodeFleece, json.size);
    FLEncoder_ConvertJSON(&e, json);
    return FLEncoder_Finish(&e, outError);
}


FLStringResult FLJSON5_ToJSON(FLString json5,
                              FLStringResult* FL_NULLABLE outErrorMessage,
                              size_t * FL_NULLABLE outErrorPos,
                              FLError* FL_NULLABLE error) FLAPI {
    alloc_slice errorMessage;
    size_t errorPos = 0;
    try {
        std::string json = ConvertJSON5((std::string((char*)json5.buf, json5.size)));
        return FLSliceResult(alloc_slice(json));
    } catch (const json5_error &x) {
        errorMessage = alloc_slice(x.what());
        errorPos = x.inputPos;
        if (error)
            *error = kFLJSONError;
    } catch (const std::exception &x) {
        errorMessage = alloc_slice(x.what());
        recordError(x, error);
    }
    if (outErrorMessage)
        *outErrorMessage = FLSliceResult(std::move(errorMessage));
    if (outErrorPos)
        *outErrorPos = errorPos;
    return {};
}


FLSliceResult FLData_Dump(FLSlice data) FLAPI {
    try {
        return FLSliceResult(alloc_slice(Value::dump(data)));
    } catchError(nullptr)
    return {nullptr, 0};
}


#pragma mark - ARRAYS:


uint32_t FLArray_Count(FLArray FL_NULLABLE a)                    FLAPI {return a ? a->count() : 0;}
bool FLArray_IsEmpty(FLArray FL_NULLABLE a)                      FLAPI {return a ? a->empty() : true;}
FLValue FL_NULLABLE FLArray_Get(FLArray FL_NULLABLE a, uint32_t index)       FLAPI {return a ? a->get(index) : nullptr;}

void FLArrayIterator_Begin(FLArray FL_NULLABLE a, FLArrayIterator* i) FLAPI {
    static_assert(sizeof(FLArrayIterator) >= sizeof(Array::iterator),"FLArrayIterator is too small");
    new (i) Array::iterator(a);
    // Note: this is safe even if a is null.
}

uint32_t FLArrayIterator_GetCount(const FLArrayIterator* i) FLAPI {
    return ((Array::iterator*)i)->count();
}

FLValue FL_NULLABLE FLArrayIterator_GetValue(const FLArrayIterator* i) FLAPI {
    return ((Array::iterator*)i)->value();
}

FLValue FL_NULLABLE FLArrayIterator_GetValueAt(const FLArrayIterator *i, uint32_t offset) FLAPI {
    return (*(Array::iterator*)i)[offset];
}

bool FLArrayIterator_Next(FLArrayIterator* i) FLAPI {
    try {
        auto& iter = *(Array::iterator*)i;
        ++iter;                 // throws if iterating past end
        return (bool)iter;
    } catchError(nullptr)
    return false;
}

static FLMutableArray FL_NULLABLE _newMutableArray(FLArray FL_NULLABLE a, FLCopyFlags flags) noexcept {
    try {
        return (MutableArray*)retain(MutableArray::newArray(a, CopyFlags(flags)));
    } catchError(nullptr)
    return nullptr;
}

FLMutableArray FL_NULLABLE FLMutableArray_New(void) FLAPI {
    return _newMutableArray(nullptr, kFLDefaultCopy);
}

FLMutableArray FL_NULLABLE FLMutableArray_NewFromJSON(FLString json, FLError* FL_NULLABLE outError) FLAPI {
    if (outError != nullptr) {
        *outError = kFLNoError;
    }

    FLDoc doc = FLDoc_FromJSON(json, outError);
    if (doc == nullptr) {
        return nullptr;
    }

    FLValue val = FLDoc_GetRoot(doc);
    if (val == nullptr || val->type() != kArray) {
        if (outError != nullptr) {
            *outError = kFLInvalidData;
        }
        FLDoc_Release(doc);
        return nullptr;
    }

    FLArray array = val->asArray();
    FLMutableArray ret = _newMutableArray(array, kFLDeepCopyImmutables);
    FLDoc_Release(doc);
    return ret;
}

FLMutableArray FL_NULLABLE FLArray_MutableCopy(FLArray FL_NULLABLE a, FLCopyFlags flags) FLAPI {
    return a ? _newMutableArray(a, flags) : nullptr;
}

FLMutableArray FL_NULLABLE FLArray_AsMutable(FLArray FL_NULLABLE a)         FLAPI {return a ? a->asMutable() : nullptr;}
FLArray FL_NULLABLE FLMutableArray_GetSource(FLMutableArray FL_NULLABLE a)  FLAPI {return a ? a->source() : nullptr;}
bool FLMutableArray_IsChanged(FLMutableArray FL_NULLABLE a)     FLAPI {return a && a->isChanged();}
void FLMutableArray_SetChanged(FLMutableArray FL_NULLABLE a, bool c)       FLAPI {if (a) a->setChanged(c);}
void FLMutableArray_Resize(FLMutableArray FL_NULLABLE a, uint32_t size)    FLAPI {a->resize(size);}

FLSlot FLMutableArray_Set(FLMutableArray a, uint32_t index)    FLAPI {return &a->setting(index);}
FLSlot FLMutableArray_Append(FLMutableArray a)                 FLAPI {return &a->appending();}

void FLMutableArray_Insert(FLMutableArray FL_NULLABLE a, uint32_t firstIndex, uint32_t count) FLAPI {
    if (a) a->insert(firstIndex, count);
}

void FLMutableArray_Remove(FLMutableArray FL_NULLABLE a, uint32_t firstIndex, uint32_t count) FLAPI {
    if(a) a->remove(firstIndex, count);
}

FLMutableArray FL_NULLABLE FLMutableArray_GetMutableArray(FLMutableArray FL_NULLABLE a, uint32_t index) FLAPI {
    return a ? a->getMutableArray(index) : nullptr;
}

FLMutableDict FL_NULLABLE FLMutableArray_GetMutableDict(FLMutableArray FL_NULLABLE a, uint32_t index) FLAPI {
    return a ? a->getMutableDict(index) : nullptr;
}


#pragma mark - DICTIONARIES:


uint32_t FLDict_Count(FLDict FL_NULLABLE d)                 FLAPI {return d ? d->count() : 0;}
bool FLDict_IsEmpty(FLDict FL_NULLABLE d)                   FLAPI {return d ? d->empty() : true;}
FLValue FL_NULLABLE FLDict_Get(FLDict FL_NULLABLE d, FLSlice keyString) FLAPI {return d ? d->get(keyString) : nullptr;}

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

void FLDictIterator_Begin(FLDict FL_NULLABLE d, FLDictIterator* i) FLAPI {
    static_assert(sizeof(FLDictIterator) >= sizeof(Dict::iterator), "FLDictIterator is too small");
    new (i) Dict::iterator(d);
    // Note: this is safe even if d is null.
}

FLValue FL_NULLABLE FLDictIterator_GetKey(const FLDictIterator* i) FLAPI {
    return ((Dict::iterator*)i)->key();
}

FLString FLDictIterator_GetKeyString(const FLDictIterator* i) FLAPI {
    return ((Dict::iterator*)i)->keyString();
}

FLValue FL_NULLABLE FLDictIterator_GetValue(const FLDictIterator* i) FLAPI {
    return ((Dict::iterator*)i)->value();
}

uint32_t FLDictIterator_GetCount(const FLDictIterator* i) FLAPI {
    return ((Dict::iterator*)i)->count();
}

bool FLDictIterator_Next(FLDictIterator* i) FLAPI {
    try {
        auto& iter = *(Dict::iterator*)i;
        ++iter;                 // throws if iterating past end
        if (iter)
            return true;
        iter.~DictIterator();
    } catchError(nullptr)
    return false;
}

void FLDictIterator_End(FLDictIterator* i) FLAPI {
    ((Dict::iterator*)i)->~DictIterator();
}


FLDictKey FLDictKey_Init(FLSlice string) FLAPI {
    FLDictKey key;
    static_assert(sizeof(FLDictKey) >= sizeof(Dict::key), "FLDictKey is too small");
    new (&key) Dict::key(string);
    return key;
}

FLSlice FLDictKey_GetString(const FLDictKey *key) FLAPI {
    auto realKey = (const Dict::key*)key;
    return realKey->string();
}

FLValue FL_NULLABLE FLDict_GetWithKey(FLDict FL_NULLABLE d, FLDictKey *k) FLAPI {
    if (!d)
        return nullptr;
    auto &key = *(Dict::key*)k;
    return d->get(key);
}


static FLMutableDict FL_NULLABLE _newMutableDict(FLDict FL_NULLABLE d, FLCopyFlags flags) noexcept {
    try {
        return (MutableDict*)retain(MutableDict::newDict(d, CopyFlags(flags)));
    } catchError(nullptr)
    return nullptr;
}

FLMutableDict FLMutableDict_New(void) FLAPI {
    return _newMutableDict(nullptr, kFLDefaultCopy);
}

FLMutableDict FL_NULLABLE FLMutableDict_NewFromJSON(FLString json, FLError* FL_NULLABLE outError) FLAPI {
    if (outError != nullptr) {
        *outError = kFLNoError;
    }

    FLDoc doc = FLDoc_FromJSON(json, outError);
    if (doc == nullptr) {
        return nullptr;
    }

    FLValue val = FLDoc_GetRoot(doc);
    if (val == nullptr || val->type() != kDict) {
        if (outError != nullptr) {
            *outError = kFLInvalidData;
        }
        FLDoc_Release(doc);
        return nullptr;
    }

    FLDict dict = val->asDict();
    FLMutableDict ret = _newMutableDict(dict, kFLDeepCopyImmutables);
    FLDoc_Release(doc);
    return ret;
}

FLMutableDict FL_NULLABLE FLDict_MutableCopy(FLDict FL_NULLABLE d, FLCopyFlags flags) FLAPI {
    return d ? _newMutableDict(d, flags) : nullptr;
}

FLMutableDict FL_NULLABLE FLDict_AsMutable(FLDict FL_NULLABLE d)           FLAPI {return d ? d->asMutable() : nullptr;}
FLDict FL_NULLABLE FLMutableDict_GetSource(FLMutableDict FL_NULLABLE d)    FLAPI {return d ? d->source() : nullptr;}
bool FLMutableDict_IsChanged(FLMutableDict FL_NULLABLE d)      FLAPI {return d && d->isChanged();}
void FLMutableDict_SetChanged(FLMutableDict FL_NULLABLE d, bool c)   FLAPI {if (d) d->setChanged(c);}

FLSlot FLMutableDict_Set(FLMutableDict d, FLString k)    FLAPI {return &d->setting(k);}

void FLMutableDict_Remove(FLMutableDict FL_NULLABLE d, FLString key) FLAPI {if(d) d->remove(key);}
void FLMutableDict_RemoveAll(FLMutableDict FL_NULLABLE d)            FLAPI {if(d) d->removeAll();}

FLMutableArray FL_NULLABLE FLMutableDict_GetMutableArray(FLMutableDict FL_NULLABLE d, FLString key) FLAPI {
    return d ? d->getMutableArray(key) : nullptr;
}

FLMutableDict FL_NULLABLE FLMutableDict_GetMutableDict(FLMutableDict FL_NULLABLE d, FLString key) FLAPI {
    return d ? d->getMutableDict(key) : nullptr;
}


//////// SHARED KEYS


FLSharedKeys FLSharedKeys_New()                            FLAPI {return retain(new SharedKeys());}
FLSharedKeys FL_NULLABLE FLSharedKeys_Retain(FLSharedKeys FL_NULLABLE sk)          FLAPI {return retain(sk);}
void FLSharedKeys_Release(FLSharedKeys FL_NULLABLE sk)                 FLAPI {release(sk);}
unsigned FLSharedKeys_Count(FLSharedKeys sk)               FLAPI {return (unsigned)sk->count();}
bool FLSharedKeys_LoadStateData(FLSharedKeys sk, FLSlice d)FLAPI {return sk->loadFrom(d);}
bool FLSharedKeys_LoadState(FLSharedKeys sk, FLValue s)    FLAPI {return sk->loadFrom(s);}
FLSliceResult FLSharedKeys_GetStateData(FLSharedKeys sk)   FLAPI {return FLSliceResult(sk->stateData());}
FLString FLSharedKeys_Decode(FLSharedKeys sk, int key)     FLAPI {return sk->decode(key);}
void FLSharedKeys_RevertToCount(FLSharedKeys sk, unsigned c) FLAPI {sk->revertToCount(c);}
void FLSharedKeys_DisableCaching(FLSharedKeys sk)          FLAPI { sk->disableCaching(); }

FLSharedKeys FLSharedKeys_NewWithRead(FLSharedKeysReadCallback callback, void* FL_NULLABLE context) FLAPI {
    return retain(new FLPersistentSharedKeys(callback, context));
}

void FLSharedKeys_WriteState(FLSharedKeys sk, FLEncoder e) FLAPI {
    assert_always(e->isFleece());
    sk->writeState(*e->fleeceEncoder());
}

int FLSharedKeys_Encode(FLSharedKeys sk, FLString keyStr, bool add) FLAPI {
    int intKey;
    if (!(add ? sk->encodeAndAdd(keyStr, intKey) : sk->encode(keyStr, intKey)))
        intKey = -1;
    return intKey;
}


FLSharedKeyScope FLSharedKeyScope_WithRange(FLSlice range, FLSharedKeys sk) FLAPI {
    return (FLSharedKeyScope) new Scope(range, sk);
}

void FLSharedKeyScope_Free(FLSharedKeyScope FL_NULLABLE scope) FLAPI {
    delete (Scope*) scope;
}

// deprecated
extern "C" {
    FL_NULLABLE FLSharedKeys FLSharedKeys_Create() FLAPI;
    FL_NULLABLE FLSharedKeys FLSharedKeys_CreateFromStateData(FLSlice) FLAPI;
}
    
FLSharedKeys FLSharedKeys_Create() FLAPI {
    return FLSharedKeys_New();
}

FL_NULLABLE FLSharedKeys FLSharedKeys_CreateFromStateData(FLSlice data) FLAPI {
    FLSharedKeys keys = FLSharedKeys_New();
    FLSharedKeys_LoadStateData(keys, data);
    return keys;
}



#pragma mark - SLOTS:


void FLSlot_SetNull(FLSlot slot)                        FLAPI {slot->set(Null());}
void FLSlot_SetBool(FLSlot slot, bool v)                FLAPI {slot->set(v);}
void FLSlot_SetInt(FLSlot slot, int64_t v)              FLAPI {slot->set(v);}
void FLSlot_SetUInt(FLSlot slot, uint64_t v)            FLAPI {slot->set(v);}
void FLSlot_SetFloat(FLSlot slot, float v)              FLAPI {slot->set(v);}
void FLSlot_SetDouble(FLSlot slot, double v)            FLAPI {slot->set(v);}
void FLSlot_SetString(FLSlot slot, FLString v)          FLAPI {slot->set(v);}
void FLSlot_SetData(FLSlot slot, FLSlice v)             FLAPI {slot->setData(v);}
void FLSlot_SetValue(FLSlot slot, FLValue v)            FLAPI {slot->set(v);}


#pragma mark - DEEP ITERATOR:


FLDeepIterator FLDeepIterator_New(FLValue FL_NULLABLE v)FLAPI {return new DeepIterator(v);}
void FLDeepIterator_Free(FLDeepIterator FL_NULLABLE i)  FLAPI {delete i;}
FLValue FL_NULLABLE FLDeepIterator_GetValue(FLDeepIterator i) FLAPI {return i->value();}
FLValue FL_NULLABLE FLDeepIterator_GetParent(FLDeepIterator i) FLAPI {return i->parent();}
FLSlice FLDeepIterator_GetKey(FLDeepIterator i)         FLAPI {return i->keyString();}
uint32_t FLDeepIterator_GetIndex(FLDeepIterator i)      FLAPI {return i->index();}
size_t FLDeepIterator_GetDepth(FLDeepIterator i)        FLAPI {return i->path().size();}
void FLDeepIterator_SkipChildren(FLDeepIterator i)      FLAPI {i->skipChildren();}

bool FLDeepIterator_Next(FLDeepIterator i) FLAPI {
    i->next();
    return i->value() != nullptr;
}

void FLDeepIterator_GetPath(FLDeepIterator i, FLPathComponent* *outPath, size_t *outDepth) FLAPI {
    static_assert(sizeof(FLPathComponent) == sizeof(DeepIterator::PathComponent),
                  "FLPathComponent does not match PathComponent");
    auto &path = i->path();
    *outPath = (FLPathComponent*) path.data();
    *outDepth = path.size();
}

FLSliceResult FLDeepIterator_GetPathString(FLDeepIterator i) FLAPI {
    return FLSliceResult(alloc_slice(i->pathString()));
}

FLSliceResult FLDeepIterator_GetJSONPointer(FLDeepIterator i) FLAPI {
    return FLSliceResult(alloc_slice(i->jsonPointer()));
}


#pragma mark - KEY-PATHS:


FLKeyPath FL_NULLABLE FLKeyPath_New(FLSlice specifier, FLError* FL_NULLABLE outError) FLAPI {
    try {
        return new Path(specifier);
    } catchError(outError)
    return nullptr;
}

void FLKeyPath_Free(FLKeyPath FL_NULLABLE path) FLAPI {
    delete path;
}

FLValue FL_NULLABLE FLKeyPath_Eval(FLKeyPath path, FLValue root) FLAPI {
    return path->eval(root);
}

FLValue FL_NULLABLE FLKeyPath_EvalOnce(FLSlice specifier, FLValue root, FLError * FL_NULLABLE outError) FLAPI {
    try {
        return Path::eval(specifier, root);
    } catchError(outError)
    return nullptr;
}

FLStringResult FLKeyPath_ToString(FLKeyPath path) FLAPI {
    return FLSliceResult(alloc_slice(std::string(*path)));
}

bool FLKeyPath_Equals(FLKeyPath path1, FLKeyPath path2) FLAPI {
    return *path1 == *path2;
}

size_t FLKeyPath_GetCount(FLKeyPath path) FLAPI {
    return path->size();
}

bool FLKeyPath_GetElement(FLKeyPath path, size_t i, FLSlice *outKey, int32_t *outIndex) FLAPI {
    if (i >= path->size())
        return false;
    auto &element = (*path)[i];
    *outKey = element.keyStr();
    *outIndex = element.index();
    return true;
}

FLKeyPath FL_NULLABLE FLKeyPath_NewEmpty(void) FLAPI {
    return new Path();
}

void FLKeyPath_AddProperty(FLKeyPath path, FLString property) FLAPI {
    if (property.size > 0)
        path->addProperty(property);
}

void FLKeyPath_AddIndex(FLKeyPath path, int index) FLAPI {
    path->addIndex(index);
}

bool FLKeyPath_AddComponents(FLKeyPath path, FLString specifier, FLError* FL_NULLABLE outError) FLAPI {
    try {
        path->addComponents(specifier);
        return true;
    } catchError(outError)
    return false;
}

void FLKeyPath_DropComponents(FLKeyPath path, size_t n) FLAPI {
    path->drop(n);
}


#pragma mark - ENCODER:




#pragma mark - BUILDER


    FLValue FLValue_NewWithFormat(const char *format, ...) {
        va_list args;
        va_start(args, format);
        auto result = FLValue_NewWithFormatV(format, args);
        va_end(args);
        return result;
    }

    FLValue FLValue_NewWithFormatV(const char *format, va_list args) {
        return std::move(builder::VBuild(format, args)).detach();
    }

    void FLMutableArray_UpdateWithFormat(FLMutableArray array, const char *format, ...) {
        va_list args;
        va_start(args, format);
        FLValue_UpdateWithFormatV(array, format, args);
        va_end(args);
    }

    void FLMutableDict_UpdateWithFormat(FLMutableDict dict, const char *format, ...) {
        va_list args;
        va_start(args, format);
        FLValue_UpdateWithFormatV(dict, format, args);
        va_end(args);
    }

    void FLValue_UpdateWithFormatV(FLValue v, const char *format, va_list args) {
        assert(FLValue_IsMutable(v));
        builder::VPut(const_cast<Value*>(v), format, args);
    }


#pragma mark - DOCUMENTS


FLDoc FLDoc_FromResultData(FLSliceResult data, FLTrust trust, FLSharedKeys FL_NULLABLE sk, FLSlice externData) FLAPI {
    return retain(new Doc(alloc_slice(data), (Doc::Trust)trust, sk, externData));
}

FLDoc FL_NULLABLE FLDoc_FromJSON(FLSlice json, FLError* FL_NULLABLE outError) FLAPI {
    try {
        return retain(Doc::fromJSON(json));
    } catchError(outError);
    return nullptr;
}

void FLDoc_Release(FLDoc FL_NULLABLE doc)                  FLAPI {release(doc);}
FLDoc FL_NULLABLE FLDoc_Retain(FLDoc FL_NULLABLE doc)      FLAPI {return retain(doc);}

FLSharedKeys FL_NULLABLE FLDoc_GetSharedKeys(FLDoc FL_NULLABLE doc)    FLAPI {return doc ? doc->sharedKeys() : nullptr;}
FLValue FL_NULLABLE FLDoc_GetRoot(FLDoc FL_NULLABLE doc)               FLAPI {return doc ? doc->root() : nullptr;}
FLSlice FLDoc_GetData(FLDoc FL_NULLABLE doc)               FLAPI {return doc ? doc->data() : slice();}

FLSliceResult FLDoc_GetAllocedData(FLDoc FL_NULLABLE doc) FLAPI {
    return doc ? FLSliceResult(doc->allocedData()) : FLSliceResult{};
}

void* FL_NULLABLE FLDoc_GetAssociated(FLDoc FL_NULLABLE doc, const char *type) FLAPI {
    return doc ? doc->getAssociated(type) : nullptr;
}

bool FLDoc_SetAssociated(FLDoc FL_NULLABLE doc, void * FL_NULLABLE pointer, const char *type) FLAPI {
    return doc && const_cast<Doc*>(doc)->setAssociated(pointer, type);
}


#pragma mark - DELTA COMPRESSION


FLSliceResult FLCreateJSONDelta(FLValue FL_NULLABLE old, FLValue FL_NULLABLE nuu) FLAPI {
    try {
        return FLSliceResult(JSONDelta::create(old, nuu));
    } catch (const std::exception&) {
        return {};
    }
}

bool FLEncodeJSONDelta(FLValue FL_NULLABLE old, FLValue FL_NULLABLE nuu, FLEncoder jsonEncoder) FLAPI {
    try {
        JSONEncoder *enc = jsonEncoder->jsonEncoder();
        precondition(enc);  //TODO: Support encoding to Fleece
        JSONDelta::create(old, nuu, *enc);
        return true;
    } catch (const std::exception &x) {
        jsonEncoder->recordException(x);
        return false;
    }
}


FLSliceResult FLApplyJSONDelta(FLValue FL_NULLABLE old, FLSlice jsonDelta, FLError * FL_NULLABLE outError) FLAPI {
    try {
        return FLSliceResult(JSONDelta::apply(old, jsonDelta));
    } catchError(outError);
    return {};
}

bool FLEncodeApplyingJSONDelta(FLValue FL_NULLABLE old, FLSlice jsonDelta, FLEncoder encoder) FLAPI {
    try {
        Encoder *enc = encoder->fleeceEncoder();
        if (!enc)
            FleeceException::_throw(EncodeError, "FLEncodeApplyingJSONDelta cannot encode JSON");
        JSONDelta::apply(old, jsonDelta, false, *enc);
        return true;
    } catch (const std::exception &x) {
        encoder->recordException(x);
        return false;
    }
}

FL_ASSUME_NONNULL_END
