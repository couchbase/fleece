//
// Delta.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "FleeceImpl.hh"
#include <string>

namespace fleece { namespace impl {
    class JSONEncoder;


    class Delta {
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

        /** Applies the (parsed) JSON delta produced by `create` to the value `old` (which must be
            equal to the `old` value originally passed to `create`) and writes the corresponding
            `nuu` value to the Fleece encoder.
            If the delta is malformed or can't be applied to `old`, throws a FleeceException. */
        static void apply(const Value *old, const Value* NONNULL delta, Encoder&);

    private:
        struct pathItem;

        Delta(JSONEncoder&);
        bool _write(const Value *old, const Value *nuu, pathItem *path);

        Delta(Encoder&);
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
