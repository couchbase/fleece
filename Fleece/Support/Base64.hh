//
// Base64.hh
//
// Copyright © 2021 Couchbase. All rights reserved.
//

#pragma once
#include "fleece/slice.hh"

namespace fleece { namespace base64 {

    /** Encodes the data in the slice as base64. */
    std::string encode(slice);

    /** Decodes Base64 data from a slice into a new alloc_slice.
        On failure returns a null slice. */
    alloc_slice decode(slice);

    /** Decodes Base64 data from receiver into output. On success returns subrange of output
        where the decoded data is. If output is too small to hold all the decoded data, returns
        a null slice. */
    slice decode(slice input, void *outputBuffer, size_t bufferSize) noexcept;

} }
