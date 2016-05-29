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

    // This is the C API! For the C++ API, see Fleece.hh.


    //////// TYPES

#ifndef FL_IMPL
    typedef const struct _FLValue* FLValue;
    typedef const struct _FLArray* FLArray;
    typedef const struct _FLDict*  FLDict;
    typedef struct _FLEncoder* FLEncoder;

    /** A simple reference to a block of memory. Does not imply ownership. */
    typedef struct {
        const void *buf;
        size_t size;
    } FLSlice;
#endif

    /** A block of memory returned from a function. The caller assumes ownership, may modify the
        bytes, and must call FLSliceFree when done. */
    typedef struct {
        void *buf;
        size_t size;
    } FLSliceResult;

    /** Frees the memory of a FLSliceResult. */
    void FLSliceFree(FLSliceResult);

    /** Types of Fleece values. Basically JSON, with the addition of Data (raw blob). */
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

    /** Returns a reference to the root value in the encoded data.
        Validates the data first; if it's invalid, returns NULL.
        Does NOT copy or take ownership of the data; the caller is responsible for keeping it
        intact. Any changes to the data will invalidate any FLValues obtained from it. */
    FLValue FLValueFromData(FLSlice data);

    /** Returns a pointer to the root value in the encoded data, without validating.
        This is a lot faster, but "undefined behavior" occurs if the data is corrupt... */
    FLValue FLValueFromTrustedData(FLSlice data);

    /** Produces a human-readable dump of the data. */
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

    /** Returns a string representation of a value. Data values are returned in raw form.
        Arrays and dictionaries don't have a representation and will return NULL. */
    FLSliceResult FLValueToString(FLValue);

    /** Encodes a Fleece value as JSON (or a JSON fragment.) Data will be base64-encoded. */
    FLSliceResult FLValueToJSON(FLValue);


    //////// ARRAY

    uint32_t FLArrayCount(FLArray);
    FLValue FLArrayGet(FLArray, uint32_t index);

    /** Opaque dictionary iterator. Put one on the stack and pass its address to
        FLArrayIteratorBegin. */
    typedef struct {
        void* _private1;
        uint32_t _private2;
        bool _private3;
        void* _private4;
    } FLArrayIterator;

    /** Initializes a FLArrayIterator struct to iterate over an array.
        Call FLArrayIteratorGetValue to get the first item, then FLArrayIteratorNext. */
    void FLArrayIteratorBegin(FLArray, FLArrayIterator*);
    FLValue FLArrayIteratorGetValue(const FLArrayIterator*);
    bool FLArrayIteratorNext(FLArrayIterator*);


    //////// DICT

    uint32_t FLDictCount(FLDict);
    FLValue FLDictGet(FLDict, FLSlice keyString);
    FLValue FLDictGetUnsorted(FLDict, FLSlice keyString);

    /** Opaque dictionary iterator. Put one on the stack and pass its address to
        FLDictIteratorBegin. */
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

    /** Opaque key for a dictionary. Put one on the stack and pass its address to FLDictKeyInit. */
    typedef struct {
        void* _private1[3];
        uint32_t _private2;
        bool _private3;
    } FLDictKey;

    void FLDictKeyInit(FLDictKey*, FLSlice string, bool cachePointers);
    FLValue FLDictGetWithKey(FLDict, FLDictKey*);

    size_t FLDictGetWithKeys(FLDict, FLDictKey[], FLValue[], size_t count);


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

    /** Directly converts JSON data to Fleece data. Error codes are actually jsonsl_error_t */
    FLSliceResult FLConvertJSON(FLSlice json, int *outError);


#ifdef __cplusplus
}
#endif
#endif /* Fleece_h */
