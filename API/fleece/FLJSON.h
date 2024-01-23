//
// FLJSON.h
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
#ifndef _FLJSON_H
#define _FLJSON_H

#include "FLBase.h"

FL_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

    // This is the C API! For the C++ API, see Fleece.hh.

    /** \defgroup json   JSON Interoperability */

     /** \name Converting to JSON
        @{
        These are convenience functions that directly return a JSON representation of a value.
        For more control over the encoding, use an \ref FLEncoder with format \ref kFLEncodeJSON. */

    /** Encodes a Fleece value as JSON (or a JSON fragment.)
        @note Any Data values will be encoded as base64-encoded strings. */
    FLEECE_PUBLIC FLStringResult FLValue_ToJSON(FLValue FL_NULLABLE) FLAPI;

    /** Encodes a Fleece value as JSON5, a more lenient variant of JSON that allows dictionary
        keys to be unquoted if they're alphanumeric. This tends to be more readable.
        @note Any Data values will be encoded as base64-encoded strings. */
    FLEECE_PUBLIC FLStringResult FLValue_ToJSON5(FLValue FL_NULLABLE) FLAPI;

    /** Most general Fleece to JSON converter.
        @param v  The Fleece value to encode
        @param json5  If true, outputs JSON5, like \ref FLValue_ToJSON5
        @param canonicalForm  If true, outputs the JSON in a consistent "canonical" form. All
                equivalent values should produce byte-for-byte identical canonical JSON.
                This is useful for creating digital signatures, for example. */
    FLEECE_PUBLIC FLStringResult FLValue_ToJSONX(FLValue FL_NULLABLE v,
                                   bool json5,
                                   bool canonicalForm) FLAPI;

    /** @} */


    /** \name Parsing JSON to Fleece Values
        @{ */

    /** Creates an FLDoc from JSON-encoded data. The data is first encoded into Fleece, and the
        Fleece data is kept by the doc; the input JSON data is no longer needed after this
        function returns. */
    NODISCARD FLEECE_PUBLIC FLDoc FLDoc_FromJSON(FLSlice json, FLError* FL_NULLABLE outError) FLAPI;

    /** Creates a new mutable Array from JSON. It is an error if the JSON is not an array.
        Its initial ref-count is 1, so a call to FLMutableArray_Release will free it.  */
    NODISCARD FLEECE_PUBLIC FLMutableArray FL_NULLABLE FLMutableArray_NewFromJSON(FLString json, FLError* FL_NULLABLE outError) FLAPI;

    /** Creates a new mutable Dict from json. It is an error if the JSON is not a dictionary/object.
        Its initial ref-count is 1, so a call to FLMutableDict_Release will free it.  */
    NODISCARD FLEECE_PUBLIC FLMutableDict FL_NULLABLE FLMutableDict_NewFromJSON(FLString json, FLError* FL_NULLABLE outError) FLAPI;

    /** Parses JSON data and writes the value(s) to the encoder as their Fleece equivalents.
        (This acts as a single write, like WriteInt; it's just that the value written is likely to
        be an entire dictionary or array.) */
    FLEECE_PUBLIC bool FLEncoder_ConvertJSON(FLEncoder, FLSlice json) FLAPI;

    /** @} */


#ifdef __cplusplus
}
#endif

FL_ASSUME_NONNULL_END

#endif // _FLJSON_H
