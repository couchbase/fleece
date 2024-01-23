//
// FLEncoder.h
//
// Copyright 2016-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#ifndef _FLENCODER_H
#define _FLENCODER_H

#include "FLBase.h"
#include <stdio.h>

FL_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

    // This is the C API! For the C++ API, see Fleece.hh.


    /** \defgroup FLEncoder   Fleece Encoders
        @{
        An FLEncoder generates encoded Fleece or JSON data. It's sort of a structured output stream,
        with nesting. There are functions for writing every type of scalar value, and for beginning
        and ending collections. To write a collection you begin it, write its values, then end it.
        (Of course a value in a collection can itself be another collection.) When writing a
        dictionary, you have to call writeKey before writing each value.
     */


    /** \name Setup and configuration
         @{ */

    /** Output formats a FLEncoder can generate. */
    typedef enum {
        kFLEncodeFleece,    ///< Fleece encoding
        kFLEncodeJSON,      ///< JSON encoding
        kFLEncodeJSON5      ///< [JSON5](http://json5.org), an extension of JSON with a more readable syntax
    } FLEncoderFormat;


    /** Creates a new encoder, for generating Fleece data. Call FLEncoder_Free when done. */
    NODISCARD FLEECE_PUBLIC FLEncoder FLEncoder_New(void) FLAPI;

    /** Creates a new encoder, allowing some options to be customized.
        @param format  The output format to generate (Fleece, JSON, or JSON5.)
        @param reserveSize  The number of bytes to preallocate for the output. (Default is 256)
        @param uniqueStrings  (Fleece only) If true, string values that appear multiple times will be written
            as a single shared value. This saves space but makes encoding slightly slower.
            You should only turn this off if you know you're going to be writing large numbers
            of non-repeated strings. (Default is true) */
    NODISCARD FLEECE_PUBLIC FLEncoder FLEncoder_NewWithOptions(FLEncoderFormat format,
                                       size_t reserveSize,
                                       bool uniqueStrings) FLAPI;

    /** Creates a new Fleece encoder that writes to a file, not to memory. */
    NODISCARD FLEECE_PUBLIC FLEncoder FLEncoder_NewWritingToFile(FILE*, bool uniqueStrings) FLAPI;

    /** Frees the space used by an encoder. */
    FLEECE_PUBLIC void FLEncoder_Free(FLEncoder FL_NULLABLE) FLAPI;

    /** Tells the encoder to use a shared-keys mapping when encoding dictionary keys. */
    FLEECE_PUBLIC void FLEncoder_SetSharedKeys(FLEncoder, FLSharedKeys FL_NULLABLE) FLAPI;

    /** Associates an arbitrary user-defined value with the encoder. */
    FLEECE_PUBLIC void FLEncoder_SetExtraInfo(FLEncoder, void* FL_NULLABLE info) FLAPI;

    /** Returns the user-defined value associated with the encoder; NULL by default. */
    FLEECE_PUBLIC void* FLEncoder_GetExtraInfo(FLEncoder) FLAPI;


    /** Resets the state of an encoder without freeing it. It can then be reused to encode
        another value. */
    FLEECE_PUBLIC void FLEncoder_Reset(FLEncoder) FLAPI;

    /** Returns the number of bytes encoded so far. */
    FLEECE_PUBLIC size_t FLEncoder_BytesWritten(FLEncoder) FLAPI;

    /** @} */


    /** \name Writing to the encoder
         @{
        @note The functions that write to the encoder do not return error codes, just a 'false'
        result on error. The actual error is attached to the encoder and can be accessed by calling
        FLEncoder_GetError or FLEncoder_End.

        After an error occurs, the encoder will ignore all subsequent writes. */

    /** Writes a `null` value to an encoder. (This is an explicitly-stored null, like the JSON
        `null`, not the "undefined" value represented by a NULL FLValue pointer.) */
    FLEECE_PUBLIC bool FLEncoder_WriteNull(FLEncoder) FLAPI;

    /** Writes an `undefined` value to an encoder. (Its value when read will not be a `NULL`
        pointer, but it can be recognized by `FLValue_GetType` returning `kFLUndefined`.)
        @note The only real use for writing undefined values is to represent "holes" in an array.
        An undefined dictionary value should be written simply by skipping the key and value. */
    FLEECE_PUBLIC bool FLEncoder_WriteUndefined(FLEncoder) FLAPI;

    /** Writes a boolean value (true or false) to an encoder. */
    FLEECE_PUBLIC bool FLEncoder_WriteBool(FLEncoder, bool) FLAPI;

    /** Writes an integer to an encoder. The parameter is typed as `int64_t` but you can pass any
        integral type (signed or unsigned) except for huge `uint64_t`s.
        The number will be written in a compact form that uses only as many bytes as necessary. */
    FLEECE_PUBLIC bool FLEncoder_WriteInt(FLEncoder, int64_t) FLAPI;

    /** Writes an unsigned integer to an encoder.
        @note This function is only really necessary for huge
        64-bit integers greater than or equal to 2^63, which can't be represented as int64_t. */
    FLEECE_PUBLIC bool FLEncoder_WriteUInt(FLEncoder, uint64_t) FLAPI;

    /** Writes a 32-bit floating point number to an encoder.
        @note As an implementation detail, if the number has no fractional part and can be
        represented exactly as an integer, it'll be encoded as an integer to save space. This is
        transparent to the reader, since if it requests the value as a float it'll be returned
        as floating-point. */
    FLEECE_PUBLIC bool FLEncoder_WriteFloat(FLEncoder, float) FLAPI;

    /** Writes a 64-bit floating point number to an encoder.
        @note As an implementation detail, the number may be encoded as a 32-bit float or even
        as an integer, if this can be done without losing precision. For example, 123.0 will be
        written as an integer, and 123.75 as a float.) */
    FLEECE_PUBLIC bool FLEncoder_WriteDouble(FLEncoder, double) FLAPI;

    /** Writes a string to an encoder. The string must be UTF-8-encoded and must not contain any
        zero bytes.
        @warning Do _not_ use this to write a dictionary key; use FLEncoder_WriteKey instead. */
    FLEECE_PUBLIC bool FLEncoder_WriteString(FLEncoder, FLString) FLAPI;

    /** Writes a timestamp to an encoder, as an ISO-8601 date string.
        @note Since neither Fleece nor JSON have a 'Date' type, the encoded string has no
        metadata that distinguishes it as a date. It's just a string.)
        @param encoder  The encoder to write to.
        @param ts  The timestamp (milliseconds since Unix epoch 1-1-1970).
        @param asUTC  If true, date is written in UTC (GMT); if false, with the local timezone.
        @return  True on success, false on error. */
    FLEECE_PUBLIC bool FLEncoder_WriteDateString(FLEncoder encoder, FLTimestamp ts, bool asUTC) FLAPI;

    /** Writes a binary data value (a blob) to an encoder. This can contain absolutely anything
        including null bytes.
        If the encoder is generating JSON, the blob will be written as a base64-encoded string. */
    FLEECE_PUBLIC bool FLEncoder_WriteData(FLEncoder, FLSlice) FLAPI;

    /** Writes a Fleece Value to an Encoder. */
    FLEECE_PUBLIC bool FLEncoder_WriteValue(FLEncoder, FLValue) FLAPI;


    /** Begins writing an array value to an encoder. This pushes a new state where each
        subsequent value written becomes an array item, until FLEncoder_EndArray is called.
        @param reserveCount  Number of array elements to reserve space for. If you know the size
            of the array, providing it here speeds up encoding slightly. If you don't know,
            just use zero. */
    FLEECE_PUBLIC bool FLEncoder_BeginArray(FLEncoder, size_t reserveCount) FLAPI;

    /** Ends writing an array value; pops back the previous encoding state. */
    FLEECE_PUBLIC bool FLEncoder_EndArray(FLEncoder) FLAPI;


    /** Begins writing a dictionary value to an encoder. This pushes a new state where each
        subsequent key and value written are added to the dictionary, until FLEncoder_EndDict is
        called.
        Before adding each value, you must call FLEncoder_WriteKey (_not_ FLEncoder_WriteString!),
        to write the dictionary key.
        @param reserveCount  Number of dictionary items to reserve space for. If you know the size
            of the dictionary, providing it here speeds up encoding slightly. If you don't know,
            just use zero. */
    FLEECE_PUBLIC bool FLEncoder_BeginDict(FLEncoder, size_t reserveCount) FLAPI;

    /** Specifies the key for the next value to be written to the current dictionary. */
    FLEECE_PUBLIC bool FLEncoder_WriteKey(FLEncoder, FLString) FLAPI;

    /** Specifies the key for the next value to be written to the current dictionary.
        The key is given as a Value, which must be a string or integer. */
    FLEECE_PUBLIC bool FLEncoder_WriteKeyValue(FLEncoder, FLValue) FLAPI;

    /** Ends writing a dictionary value; pops back the previous encoding state. */
    FLEECE_PUBLIC bool FLEncoder_EndDict(FLEncoder) FLAPI;


    /** Writes raw data directly to the encoded output.
        (This is not the same as \ref FLEncoder_WriteData, which safely encodes a blob.)
        @warning **Do not call this** unless you really know what you're doing ...
        it's quite unsafe, and only used for certain advanced purposes. */
    FLEECE_PUBLIC bool FLEncoder_WriteRaw(FLEncoder, FLSlice) FLAPI;

    /** @} */


    /** \name Finishing up
         @{ */

    /** Ends encoding; if there has been no error, it returns the encoded Fleece data packaged in
        an FLDoc. (This function does not support JSON encoding.)
        This does not free the FLEncoder; call FLEncoder_Free (or FLEncoder_Reset) next. */
    NODISCARD
    FLEECE_PUBLIC FLDoc FL_NULLABLE FLEncoder_FinishDoc(FLEncoder, FLError* FL_NULLABLE outError) FLAPI;

    /** Ends encoding; if there has been no error, it returns the encoded data, else null.
        This does not free the FLEncoder; call FLEncoder_Free (or FLEncoder_Reset) next. */
    NODISCARD
    FLEECE_PUBLIC FLSliceResult FLEncoder_Finish(FLEncoder, FLError* FL_NULLABLE outError) FLAPI;

    /** @} */


    /** \name Error handling
         @{ */

    /** Returns the error code of an encoder, or NoError (0) if there's no error. */
    FLEECE_PUBLIC FLError FLEncoder_GetError(FLEncoder) FLAPI;

    /** Returns the error message of an encoder, or NULL if there's no error. */
    FLEECE_PUBLIC const char* FL_NULLABLE FLEncoder_GetErrorMessage(FLEncoder) FLAPI;

    /** @} */
    /** @} */

#ifdef __cplusplus
}
#endif

FL_ASSUME_NONNULL_END

#endif // _FLENCODER_H
