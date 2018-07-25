//
// Delta.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once

namespace fleece {
    class Value;
    class Encoder;
    class JSONEncoder;


    /** Returns JSON that describes the changes to turn the value `old` into `nuu`.
        If the values are equal, returns nullslice. */
    alloc_slice CreateDelta(const Value* old, const Value* nuu, bool json5 =false);

    /** Writes JSON that describes the changes to turn the value `old` into `nuu`.
        If the values are equal, writes nothing and returns false. */
    bool CreateDelta(const Value* old, const Value* nuu, JSONEncoder&);


    /** Applies the JSON data created by `CreateDelta` to the value `old`, which must be equal
        to the `old` value originally passed to `CreateDelta`, and returns a Fleece document
        equal to the original `nuu` value. */
    alloc_slice ApplyDelta(const Value *old, slice jsonDelta, bool isJSON5 =false);

    /** Applies the (parsed) JSON data created by `CreateDelta` to the value `old`, which must be
        equal to the `old` value originally passed to `CreateDelta`, and writes the corresponding
        `nuu` value to the encoder. */
    void ApplyDelta(const Value *old, const Value* NONNULL delta, Encoder&);


    // Set this to true to create deltas compatible with JsonDiffPatch
    extern bool gCompatibleDeltas;
}
