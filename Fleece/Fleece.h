//
//  Fleece.h
//  Fleece
//
//  Created by Jens Alfke on 5/13/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//

#pragma once
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


    /** A block of memory returned from an API call. The caller takes ownership, may modify the
        bytes, and must call FLSliceFree when done. */
    typedef struct {
        void *buf;
        size_t size;
    } FLSliceResult;


    /** Frees the memory of a FLSliceResult. */
    void FLSliceFree(FLSliceResult);


    /** Types of Fleece values. Basically JSON, with the addition of Data (raw blob). */
    typedef enum {
        kFLUndefined = -1,  // Type of a NULL FLValue (i.e. no such value)
        kFLNull = 0,
        kFLBoolean,
        kFLNumber,
        kFLString,
        kFLData,
        kFLArray,
        kFLDict
    } FLValueType;


    typedef enum {
        NoError = 0,
        MemoryError,        // Out of memory, or allocation failed
        OutOfRange,         // Array index or iterator out of range
        InvalidData,        // Bad input data (NaN, non-string key, etc.)
        EncodeError,        // Structural error encoding (missing value, too many ends, etc.)
        JSONError,          // Error parsing JSON
        UnknownValue,       // Unparseable data in a Value (corrupt? Or from some distant future?)
        InternalError,      // Something that shouldn't happen
    } FLError;


    //////// VALUE

    /** Returns a reference to the root value in the encoded data.
        Validates the data first; if it's invalid, returns NULL.
        Does NOT copy or take ownership of the data; the caller is responsible for keeping it
        intact. Any changes to the data will invalidate any FLValues obtained from it. */
    FLValue FLValueFromData(FLSlice data, FLError *outError);

    /** Returns a pointer to the root value in the encoded data, without validating.
        This is a lot faster, but "undefined behavior" occurs if the data is corrupt... */
    FLValue FLValueFromTrustedData(FLSlice data, FLError *outError);

    /** Directly converts JSON data to Fleece data. You can then call FLValueFromTrustedData to
        get the root as a Value. */
    FLSliceResult FLConvertJSON(FLSlice json, FLError *outError);

    /** Produces a human-readable dump of the Value stored in the data. */
    FLSliceResult FLDataDump(FLSlice data);

    /** Returns the data type of an arbitrary Value. 
        (If the value is a NULL pointer, returns kFLUndefined.) */
    FLValueType FLValueGetType(FLValue);

    // Value accessors -- safe to call even if the value is NULL.

    bool FLValueIsInteger(FLValue);
    bool FLValueIsUnsigned(FLValue);
    bool FLValueIsDouble(FLValue);

    bool FLValueAsBool(FLValue);
    int64_t FLValueAsInt(FLValue);
    uint64_t FLValueAsUnsigned(FLValue);
    float FLValueAsFloat(FLValue);
    double FLValueAsDouble(FLValue);
    FLSlice FLValueAsString(FLValue);

    /** If a FLValue represents an array, returns it cast to FLArray, else NULL. */
    FLArray FLValueAsArray(FLValue);

    /** If a FLValue represents a dictionary, returns it as an FLDict, else NULL. */
    FLDict FLValueAsDict(FLValue);

    /** Returns a string representation of a value. Data values are returned in raw form.
        Arrays and dictionaries don't have a representation and will return NULL. */
    FLSliceResult FLValueToString(FLValue);

    /** Encodes a Fleece value as JSON (or a JSON fragment.) 
        Any Data values will become base64-encoded JSON strings. */
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

    void FLEncoderFree(FLEncoder);

    /** Resets the state of an encoder without freeing it. It can then be reused to encode
        another value. */
    void FLEncoderReset(FLEncoder);

    // Note: The functions that write to the encoder do not return error codes, just a 'false'
    // result on error. The actual error is attached to the encoder and can be accessed by calling
    // FLEncoderGetError or FLEncoderEnd.
    // After an error occurs, the encoder will ignore all subsequent writes.

    bool FLEncoderWriteNull(FLEncoder);
    bool FLEncoderWriteBool(FLEncoder, bool);
    bool FLEncoderWriteInt(FLEncoder, int64_t);
    bool FLEncoderWriteUInt(FLEncoder, uint64_t);
    bool FLEncoderWriteFloat(FLEncoder, float);
    bool FLEncoderWriteDouble(FLEncoder, double);
    bool FLEncoderWriteString(FLEncoder, FLSlice);
    bool FLEncoderWriteData(FLEncoder, FLSlice);

    bool FLEncoderBeginArray(FLEncoder, size_t reserveCount);
    bool FLEncoderEndArray(FLEncoder);

    bool FLEncoderBeginDict(FLEncoder, size_t reserveCount);
    bool FLEncoderWriteKey(FLEncoder, FLSlice);
    bool FLEncoderEndDict(FLEncoder);

    /** Ends encoding; if there has been no error, returns the encoded data, else null.
        This does not free the FLEncoder; call FLEncoderFree (or FLEncoderReset) next. */
    FLSliceResult FLEncoderFinish(FLEncoder, FLError*);

    /** Returns the error code (if any) of an encoder. */
    FLError FLEncoderGetError(FLEncoder e);

    /** Returns the error message (if any) of an encoder. */
    const char* FLEncoderGetErrorMessage(FLEncoder e);

#ifdef __cplusplus
}
#endif
