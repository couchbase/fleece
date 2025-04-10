//
// Base64.hh
//
// Copyright 2021-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
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
    slice decode(slice input, void *outputBuffer LIFETIMEBOUND, size_t bufferSize) noexcept;

} }
