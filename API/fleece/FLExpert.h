//
// FLExpert.h
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
#ifndef _FLOBSCURE_H
#define _FLOBSCURE_H

#include "FLValue.h"

FL_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

    // VOLATILE API: FLExpert methods are meant for internal use, and will be removed
    // in a future release

    // This is the C API! For the C++ API, see Fleece.hh.


    /** \defgroup Obscure  Rarely-needed or advanced functions
        @{ */

    /** For use with \ref FLDoc_FromResultData. This option prevents the function from parsing the
        data at all; you are responsible for locating the FLValues in it.
        This is for the case where you have trusted data in a custom format that contains Fleece-
        encoded data within it. You still need an FLDoc to access the data safely (especially to
        retain FLValues), but it can't be parsed as-is. */
    #define kFLTrustedDontParse FLTrust(-1)

    /** \name  Delta Compression
     @{
        These functions implement a fairly-efficient "delta" encoding that encapsulates the changes
        needed to transform one Fleece value into another. The delta is expressed in JSON form.

        A delta can be stored or transmitted
        as an efficient way to produce the second value, when the first is already present. Deltas
        are frequently used in version-control systems and efficient network protocols.
     */

    /** Returns JSON that encodes the changes to turn the value `old` into `nuu`.
        (The format is documented in Fleece.md, but you should treat it as a black box.)
        @param old  A value that's typically the old/original state of some data.
        @param nuu  A value that's typically the new/changed state of the `old` data.
        @return  JSON data representing the changes from `old` to `nuu`, or NULL on
                    (extremely unlikely) failure. */
    NODISCARD FLEECE_PUBLIC FLSliceResult FLCreateJSONDelta(FLValue FL_NULLABLE old,
                                    FLValue FL_NULLABLE nuu) FLAPI;

    /** Writes JSON that describes the changes to turn the value `old` into `nuu`.
        (The format is documented in Fleece.md, but you should treat it as a black box.)
        @param old  A value that's typically the old/original state of some data.
        @param nuu  A value that's typically the new/changed state of the `old` data.
        @param jsonEncoder  An encoder to write the JSON to. Must have been created using
                `FLEncoder_NewWithOptions`, with JSON or JSON5 format.
        @return  True on success, false on (extremely unlikely) failure. */
    NODISCARD FLEECE_PUBLIC bool FLEncodeJSONDelta(FLValue FL_NULLABLE old,
                           FLValue FL_NULLABLE nuu,
                           FLEncoder jsonEncoder) FLAPI;


    /** Applies the JSON data created by `CreateJSONDelta` to the value `old`, which must be equal
        to the `old` value originally passed to `FLCreateJSONDelta`, and returns a Fleece document
        equal to the original `nuu` value.
        @param old  A value that's typically the old/original state of some data. This must be
                    equal to the `old` value used when creating the `jsonDelta`.
        @param jsonDelta  A JSON-encoded delta created by `FLCreateJSONDelta` or `FLEncodeJSONDelta`.
        @param outError  On failure, error information will be stored where this points, if non-null.
        @return  The corresponding `nuu` value, encoded as Fleece, or null if an error occurred. */
    NODISCARD FLEECE_PUBLIC FLSliceResult FLApplyJSONDelta(FLValue FL_NULLABLE old,
                                   FLSlice jsonDelta,
                                   FLError* FL_NULLABLE outError) FLAPI;

    /** Applies the (parsed) JSON data created by `CreateJSONDelta` to the value `old`, which must be
        equal to the `old` value originally passed to `FLCreateJSONDelta`, and writes the corresponding
        `nuu` value to the encoder.
        @param old  A value that's typically the old/original state of some data. This must be
                    equal to the `old` value used when creating the `jsonDelta`.
        @param jsonDelta  A JSON-encoded delta created by `FLCreateJSONDelta` or `FLEncodeJSONDelta`.
        @param encoder  A Fleece encoder to write the decoded `nuu` value to. (JSON encoding is not
                    supported.)
        @return  True on success, false on error; call `FLEncoder_GetError` for details. */
    FLEECE_PUBLIC bool FLEncodeApplyingJSONDelta(FLValue FL_NULLABLE old,
                                   FLSlice jsonDelta,
                                   FLEncoder encoder) FLAPI;
    /** @} */


    /** \name  Shared Keys
        @{
        FLSharedKeys represents a mapping from short strings to small integers in the range
        [0...2047]. It's used by FLDict to abbreviate dictionary keys. A shared key can be stored in
        a fixed two bytes and is faster to compare against. However, the same mapping has to be used
        when encoding and when accessing the Dict.

        To use shared keys:
        * Call \ref FLSharedKeys_New to create a new empty mapping.
        * After creating an FLEncoder, call \ref FLEncoder_SetSharedKeys so dictionary keys will
          be added to the mapping and written in integer form.
        * When loading Fleece data, use \ref FLDoc_FromResultData and pass the FLSharedKeys as
          a parameter.
        * Save the mapping somewhere by calling \ref FLSharedKeys_GetStateData or
          \ref FLSharedKeys_WriteState.
        * You can later reconstitute the mapping by calling \ref FLSharedKeys_LoadStateData
          or \ref FLSharedKeys_LoadState on a new empty instance.
        */

    /** Creates a new empty FLSharedKeys object, which must eventually be released. */
    NODISCARD FLEECE_PUBLIC FLSharedKeys FLSharedKeys_New(void) FLAPI;

    typedef bool (*FLSharedKeysReadCallback)(void* FL_NULLABLE context, FLSharedKeys);

    NODISCARD FLEECE_PUBLIC FLSharedKeys FLSharedKeys_NewWithRead(FLSharedKeysReadCallback,
                                          void* FL_NULLABLE context) FLAPI;

    /** Returns a data blob containing the current state (all the keys and their integers.) */
    NODISCARD FLEECE_PUBLIC FLSliceResult FLSharedKeys_GetStateData(FLSharedKeys) FLAPI;

    /** Updates an FLSharedKeys with saved state data created by \ref FLSharedKeys_GetStateData.
        Returns true if new keys were added, false if not. */
    FLEECE_PUBLIC bool FLSharedKeys_LoadStateData(FLSharedKeys, FLSlice) FLAPI;

    /** Writes the current state to a Fleece encoder as a single value,
        which can later be decoded and passed to \ref FLSharedKeys_LoadState. */
    FLEECE_PUBLIC void FLSharedKeys_WriteState(FLSharedKeys, FLEncoder) FLAPI;

    /** Updates an FLSharedKeys object with saved state, a Fleece value previously written by
        \ref FLSharedKeys_WriteState. */
    NODISCARD FLEECE_PUBLIC bool FLSharedKeys_LoadState(FLSharedKeys, FLValue) FLAPI;

    /** Maps a key string to a number in the range [0...2047], or returns -1 if it isn't mapped.
        If the key doesn't already have a mapping, and the `add` flag is true,
        a new mapping is assigned and returned.
        However, the `add` flag has no effect if the key is unmappable (is longer than 16 bytes
        or contains non-identifier characters), or if all available integers have been assigned. */
    FLEECE_PUBLIC int FLSharedKeys_Encode(FLSharedKeys, FLString, bool add) FLAPI;

    /** Returns the key string that maps to the given integer `key`, else NULL. */
    FLEECE_PUBLIC FLString FLSharedKeys_Decode(FLSharedKeys, int key) FLAPI;

    /** Returns the number of keys in the mapping. This number increases whenever the mapping
        is changed, and never decreases. */
    FLEECE_PUBLIC unsigned FLSharedKeys_Count(FLSharedKeys) FLAPI;

    /** Reverts an FLSharedKeys by "forgetting" any keys added since it had the count `oldCount`. */
    FLEECE_PUBLIC void FLSharedKeys_RevertToCount(FLSharedKeys, unsigned oldCount) FLAPI;

    /** Disable caching of the SharedKeys.. */
    FLEECE_PUBLIC void FLSharedKeys_DisableCaching(FLSharedKeys) FLAPI;

    /** Increments the reference count of an FLSharedKeys. */
    NODISCARD FLEECE_PUBLIC FLSharedKeys FL_NULLABLE FLSharedKeys_Retain(FLSharedKeys FL_NULLABLE) FLAPI;

    /** Decrements the reference count of an FLSharedKeys, freeing it when it reaches zero. */
    FLEECE_PUBLIC void FLSharedKeys_Release(FLSharedKeys FL_NULLABLE) FLAPI;


    typedef struct _FLSharedKeyScope* FLSharedKeyScope;

    /** Registers a range of memory containing Fleece data that uses the given shared keys.
        This allows Dict accessors to look up the values of shared keys. */
    NODISCARD FLEECE_PUBLIC FLSharedKeyScope FLSharedKeyScope_WithRange(FLSlice range, FLSharedKeys) FLAPI;

    /** Unregisters a scope created by \ref FLSharedKeyScope_WithRange. */
    FLEECE_PUBLIC void FLSharedKeyScope_Free(FLSharedKeyScope FL_NULLABLE) FLAPI;

    /** @} */


    /** \name Parsing Fleece Data Directly
     @{ */

    /** Returns a pointer to the root value in the encoded data, or NULL if validation failed.
        You should generally use an \ref FLDoc instead; it's safer. Here's why:

        On the plus side, \ref FLValue_FromData is _extremely_ fast: it allocates no memory,
        only scans enough of the data to ensure it's valid (and if `trust` is set to `kFLTrusted`,
        it doesn't even do that.)

        But it's potentially _very_ dangerous: the FLValue, and all values found through it, are
        only valid as long as the input `data` remains intact and unchanged. If you violate
        that, the values will be pointing to garbage and Bad Things will happen when you access
        them...*/
    FLEECE_PUBLIC FLValue FL_NULLABLE FLValue_FromData(FLSlice data, FLTrust trust) FLAPI FLPURE;

    /** @} */


    /** \name JSON
     @{ */

    /** Converts valid JSON5 <https://json5.org> to JSON. Among other things, it converts single
        quotes to double, adds missing quotes around dictionary keys, removes trailing commas,
        and removes comments.
        @note If given invalid JSON5, it will _usually_ return an error, but may just ouput
              comparably invalid JSON, in which case the caller's subsequent JSON parsing will
              detect the error. The types of errors it overlooks tend to be subtleties of string
              or number encoding.
        @param json5  The JSON5 to parse
        @param outErrorMessage  On failure, the error message will be stored here (if not NULL.)
                        As this is a \ref FLStringResult, you will be responsible for freeing it.
        @param outErrorPos  On a parse error, the byte offset in the input where the error occurred
                        will be stored here (if it's not NULL.)
        @param outError  On failure, the error code will be stored here (if it's not NULL.)
        @return  The converted JSON. */
    NODISCARD FLEECE_PUBLIC FLStringResult FLJSON5_ToJSON(FLString json5,
                                  FLStringResult* FL_NULLABLE outErrorMessage,
                                  size_t* FL_NULLABLE outErrorPos,
                                  FLError* FL_NULLABLE outError) FLAPI;

    /** Directly converts JSON data to Fleece-encoded data. Not commonly needed.
        Prefer \ref FLDoc_FromJSON instead. */
    NODISCARD FLEECE_PUBLIC FLSliceResult FLData_ConvertJSON(FLSlice json, FLError* FL_NULLABLE outError) FLAPI;

    /** @} */


    /** \name Encoder
     @{ */

    /** Tells the encoder to logically append to the given Fleece document, rather than making a
        standalone document. Any calls to FLEncoder_WriteValue() where the value points inside the
        base data will write a pointer back to the original value.
        The resulting data returned by FLEncoder_FinishDoc() will *NOT* be standalone; it can only
        be used by first appending it to the base data.
        @param e  The FLEncoder affected.
        @param base  The base document to create an amendment of.
        @param reuseStrings  If true, then writing a string that already exists in the base will
                    just create a pointer back to the original. But the encoder has to scan the
                    base for strings first.
        @param externPointers  If true, pointers into the base will be marked with the `extern`
                    flag. This allows them to be resolved using the `FLResolver_Begin` function,
                    so that when the delta is used the base document can be anywhere in memory,
                    not just immediately preceding the delta document. */
    FLEECE_PUBLIC void FLEncoder_Amend(FLEncoder e, FLSlice base,
                         bool reuseStrings, bool externPointers) FLAPI;

    /** Returns the `base` value passed to FLEncoder_Amend. */
    FLEECE_PUBLIC FLSlice FLEncoder_GetBase(FLEncoder) FLAPI;

    /** Tells the encoder not to write the two-byte Fleece trailer at the end of the data.
        This is only useful for certain special purposes. */
    FLEECE_PUBLIC void FLEncoder_SuppressTrailer(FLEncoder) FLAPI;

    /** Returns the byte offset in the encoded data where the next value will be written.
        (Due to internal buffering, this is not the same as FLEncoder_BytesWritten.) */
    FLEECE_PUBLIC size_t FLEncoder_GetNextWritePos(FLEncoder) FLAPI;

    #define kFLNoWrittenValue INTPTR_MIN

    /** Returns an opaque reference to the last complete value written to the encoder, if possible.
        Fails (returning kFLNoWrittenValue) if nothing has been written, or if the value is inline
        and can't be referenced this way -- that only happens with small scalars or empty
        collections. */
    FLEECE_PUBLIC intptr_t FLEncoder_LastValueWritten(FLEncoder) FLAPI;

    /** Writes another reference (a "pointer") to an already-written value, given a reference previously
        returned from \ref FLEncoder_LastValueWritten. The effect is exactly the same as if you wrote the
        entire value again, except that the size of the encoded data only grows by 4 bytes.
        Returns false if the reference couldn't be written. */
    FLEECE_PUBLIC bool FLEncoder_WriteValueAgain(FLEncoder, intptr_t preWrittenValue) FLAPI;

    /** Returns the data written so far as a standalone Fleece document, whose root is the last
        value written. You can continue writing, and the final output returned by \ref FLEncoder_Finish will
        consist of everything after this point. That second part can be used in the future by loading it
        as an `FLDoc` with the first part as its `extern` reference. */
    NODISCARD FLEECE_PUBLIC FLSliceResult FLEncoder_Snip(FLEncoder) FLAPI;

    /** Finishes encoding the current item, and returns its offset in the output data. */
    NODISCARD FLEECE_PUBLIC size_t FLEncoder_FinishItem(FLEncoder) FLAPI;

    /** In a JSON encoder, adds a newline ('\n') and prepares to start encoding another
        top-level object. The encoder MUST be not be within an array or dict.
        Has no effect in a Fleece encoder. */
    FLEECE_PUBLIC void FLJSONEncoder_NextDocument(FLEncoder) FLAPI;


    /** @} */


    /** \name Debugging Functions
        @{ */

    /** Debugging function that returns a C string of JSON.
        Does not free the string's memory! */
    FLEECE_PUBLIC const char* FL_NULLABLE FLDump(FLValue FL_NULLABLE) FLAPI;

    /** Debugging function that parses Fleece data and returns a C string of JSON.
        Does not free the string's memory! */
    FLEECE_PUBLIC const char* FL_NULLABLE FLDumpData(FLSlice data) FLAPI;

    /** Produces a human-readable dump of Fleece-encoded data.
        This is only useful if you already know, or want to learn, the encoding format. */
    FLEECE_PUBLIC FLStringResult FLData_Dump(FLSlice data) FLAPI;

    /** @} */


    /** @} */

#ifdef __cplusplus
}
#endif

FL_ASSUME_NONNULL_END

#endif // _FLOBSCURE_H
