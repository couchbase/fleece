//
// Base64.cc
//
// Copyright 2021-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "Base64.hh"
#include "encode.h"
#include "decode.h"
#include "betterassert.hh"

namespace fleece { namespace base64 {

    std::string encode(slice data) {
        std::string str;
        // 3 bytes -> 4 chars : ceil(data.size / 3) * 4
        size_t strLen = ((data.size + 2) / 3) * 4;
        str.resize(strLen);
        char *dst = &str[0];
        ::base64::encoder enc;
        enc.set_chars_per_line(0);
        size_t written = enc.encode(data.buf, data.size, dst);
        written += enc.encode_end(dst + written);
        assert(written == strLen);
        (void)written;  // avoid compiler warning in release build when 'assert' is a no-op
        return str;
    }


    alloc_slice decode(slice b64) {
        // 4 chars -> 3 bytes : ceil(b64.size / 4) * 3
        size_t expectedLen = ((b64.size + 3) / 4 * 3);
        alloc_slice result(expectedLen);
        slice decoded = decode(b64, (void*)result.buf, result.size);
        if (decoded.size == 0)
            return nullslice;
        assert(decoded.size <= expectedLen);
        result.resize(decoded.size);
        return result;
    }


    slice decode(slice b64, void *outputBuffer, size_t bufferSize) noexcept {
        size_t expectedLen = (b64.size + 3) / 4 * 3;
        if (expectedLen > bufferSize)
            return nullslice;
        ::base64::decoder dec;
        size_t len = dec.decode(b64.buf, b64.size, outputBuffer);
        assert(len <= bufferSize);
        return slice(outputBuffer, len);
    }

} }
