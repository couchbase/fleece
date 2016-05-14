//
//  Fleece.h
//  Fleece
//
//  Created by Jens Alfke on 5/13/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#ifndef Fleece_h
#define Fleece_h
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

    // This is the C API. For the C++ API, see Fleece.hh.


    //////// TYPES

#ifndef FL_IMPL
    typedef const struct _FLValue* FLValue;
    typedef const struct _FLArray* FLArray;
    typedef const struct _FLDict*  FLDict;
    typedef struct _FLEncoder* FLEncoder;

    typedef struct {
        const void *buf;
        size_t size;
    } FLSlice;
#endif

    typedef struct {
        void *buf;
        size_t size;
    } FLSliceResult;

    void FLSliceFree(FLSliceResult);

    typedef enum {
        kFLNull = 0,
        kFLBoolean,
        kFLNumber,
        kFLString,
        kFLData,
        kFLArray,
        kFLDict
    } FLValueType;


    //////// VALUE

    FLValue FLValueFromData(FLSlice data);
    FLValue FLValueFromTrustedData(FLSlice data);

    FLSliceResult FLDataDump(FLSlice data);

    FLValueType FLValueGetType(FLValue);
    bool FLValueIsInteger(FLValue);
    bool FLValueIsUnsigned(FLValue);
    bool FLValueIsDouble(FLValue);

    bool FLValueAsBool(FLValue);
    int64_t FLValueAsInt(FLValue);
    uint64_t FLValueAsUnsigned(FLValue);
    float FLValueAsFloat(FLValue);
    double FLValueAsDouble(FLValue);
    FLSlice FLValueAsString(FLValue);
    FLArray FLValueAsArray(FLValue);
    FLDict FLValueAsDict(FLValue);

    FLSliceResult FLValueToString(FLValue);
    FLSliceResult FLValueToJSON(FLValue);


    //////// ARRAY

    uint32_t FLArrayCount(FLArray);
    FLValue FLArrayGet(FLArray, uint32_t index);

    typedef struct {
        void* _private1;
        uint32_t _private2;
        bool _private3;
        void* _private4;
    } FLArrayIterator;

    void FLArrayIteratorBegin(FLArray, FLArrayIterator*);
    FLValue FLArrayIteratorGetValue(const FLArrayIterator*);
    bool FLArrayIteratorNext(FLArrayIterator*);


    //////// DICT

    uint32_t FLDictCount(FLDict);
    FLValue FLDictGet(FLDict, FLSlice keyString);
    FLValue FLDictGetUnsorted(FLDict, FLSlice keyString);

    typedef struct {
        void* _private1;
        uint32_t _private2;
        bool _private3;
        void* _private4;
        void* _private5;
    } FLDictIterator;

    void FLDictIteratorBegin(FLDict, FLDictIterator*);
    FLValue FLDictIteratorGetKey(const FLDictIterator*);
    FLValue FLDictIteratorGetValue(const FLDictIterator*);
    bool FLDictIteratorNext(FLDictIterator*);

    typedef struct {
        void* _private1[3];
        uint32_t _private2;
    } FLDictKey;

    void FLDictKeyInit(FLDictKey*, FLSlice string);
    FLValue FLDictGetWithKey(FLDict, FLDictKey*);


    //////// ENCODER

    FLEncoder FLEncoderNew(void);
    FLEncoder FLEncoderNewWithOptions(size_t reserveSize, bool uniqueStrings, bool sortKeys);
    void FLEncoderReset(FLEncoder);
    void FLEncoderFree(FLEncoder);

    void FLEncoderWriteNull(FLEncoder);
    void FLEncoderWriteBool(FLEncoder, bool);
    void FLEncoderWriteInt(FLEncoder, int64_t);
    void FLEncoderWriteUInt(FLEncoder, uint64_t);
    void FLEncoderWriteFloat(FLEncoder, float);
    void FLEncoderWriteDouble(FLEncoder, double);
    void FLEncoderWriteString(FLEncoder, FLSlice);
    void FLEncoderWriteData(FLEncoder, FLSlice);

    void FLEncoderBeginArray(FLEncoder, size_t reserveCount);
    void FLEncoderEndArray(FLEncoder);

    void FLEncoderBeginDict(FLEncoder, size_t reserveCount);
    void FLEncoderWriteKey(FLEncoder, FLSlice);
    void FLEncoderEndDict(FLEncoder);

    FLSliceResult FLEncoderEnd(FLEncoder);

    /** Directly converts JSON data to Fleece data. */
    FLSliceResult FLConvertJSON(FLSlice json, int *outError);


#ifdef __cplusplus
}
#endif
#endif /* Fleece_h */
