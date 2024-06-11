//
// JSONDelta.hh
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "FleeceImpl.hh"
#include <string>

namespace fleece { namespace impl {
    class JSONEncoder;


    class JSONDelta {
    public:

        /** Returns JSON that describes the changes to turn the value `old` into `nuu`.
            If the values are equal, returns nullslice. */
            static alloc_slice create(const Value *old, const Value *nuu, bool json5 =false);

        /** Writes JSON that describes the changes to turn the value `old` into `nuu`.
            If the values are equal, writes nothing and returns false. */
        static bool create(const Value *old, const Value *nuu, JSONEncoder&);


        /** Applies the JSON delta created by `create` to the value `old` (which must be equal
            to the `old` value originally passed to `create`) and returns a Fleece document
            equal to the original `nuu` value.
            If the delta is malformed or can't be applied to `old`, throws a FleeceException. */
        static alloc_slice apply(const Value *old, slice jsonDelta, bool isJSON5 =false);

        /** Applies the JSON delta created by `create` to the value `old` (which must be equal
            to the `old` value originally passed to `create`) and writes the corresponding
            `nuu` value to the Fleece encoder.
            If the delta is malformed or can't be applied to `old`, throws a FleeceException. */
        static void apply(const Value *old, slice jsonDelta, bool isJSON5, Encoder&);

        /** Minimum byte length of strings that will be considered for diffing (default 60) */
        static size_t gMinStringDiffLength;

        /** Maximum time (in seconds) that the string-diff algorithm is allowed to run
            (default 0.25) */
        static float gTextDiffTimeout;

    private:
        struct pathItem;

        JSONDelta(JSONEncoder&);
        bool _write(const Value *old, const Value *nuu, pathItem *path);

        JSONDelta(Encoder&);
        void _apply(const Value *old, const Value* NONNULL delta);
        void _applyArray(const Value* old, const Array* NONNULL delta);
        void _patchArray(const Array* NONNULL old, const Dict* NONNULL delta);
        void _patchDict(const Dict* NONNULL old, const Dict* NONNULL delta);

        void writePath(pathItem*);
        static bool isDeltaDeletion(const Value *delta);
        static std::string createStringDelta(slice oldStr, slice nuuStr);
        static std::string applyStringDelta(slice oldStr, slice diff);

        JSONEncoder* _encoder;
        Encoder* _decoder;
    };
} }
