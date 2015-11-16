//
//  varint.cc
//  Fleece
//
//  Created by Jens Alfke on 3/31/14.
//  Copyright (c) 2014 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "varint.hh"
#include "slice.hh"
#include "Endian.h"
#include <assert.h>
#include <stdio.h>


namespace fleece {

size_t SizeOfVarInt(uint64_t n) {
    size_t size = 1;
    while (n >= 0x80) {
        size++;
        n >>= 7;
    }
    return size;
}


size_t PutUVarInt(void *buf, uint64_t n) {
    uint8_t* dst = (uint8_t*)buf;
    while (n >= 0x80) {
        *dst++ = (n & 0xFF) | 0x80;
        n >>= 7;
    }
    *dst++ = (uint8_t)n;
    return dst - (uint8_t*)buf;
}


size_t GetUVarInt(slice buf, uint64_t *n) {
    uint64_t result = 0;
    int shift = 0;
    for (int i = 0; i < buf.size; i++) {
        uint8_t byte = ((const uint8_t*)buf.buf)[i];
        result |= (uint64_t)(byte & 0x7f) << shift;
        if (byte >= 0x80) {
            shift += 7;
        } else {
            if (i > 9 || (i == 9 && byte > 1))
                return 0; // Numeric overflow
            *n = result;
            return i + 1;
        }
    }
    return 0; // buffer too short
}

size_t GetUVarInt32(slice buf, uint32_t *n) {
    uint64_t n64;
    size_t size = GetUVarInt(buf, &n64);
    if (size == 0 || n64 > UINT32_MAX) // Numeric overflow
        return 0;
    *n = (uint32_t)n64;
    return size;
}


bool ReadUVarInt(slice *buf, uint64_t *n) {
    if (buf->size == 0)
        return false;
    size_t bytesRead = GetUVarInt(*buf, n);
    if (bytesRead == 0)
        return false;
    buf->moveStart(bytesRead);
    return true;
}

bool ReadUVarInt32(slice *buf, uint32_t *n) {
    if (buf->size == 0)
        return false;
    size_t bytesRead = GetUVarInt32(*buf, n);
    if (bytesRead == 0)
        return false;
    buf->moveStart(bytesRead);
    return true;
}


bool WriteUVarInt(slice *buf, uint64_t n) {
    if (buf->size < kMaxVarintLen64 && buf->size < SizeOfVarInt(n))
        return false;
    size_t bytesWritten = PutUVarInt((void*)buf->buf, n);
    buf->moveStart(bytesWritten);
    return true;
}

    //////// Length-encoded ints:

    int64_t GetIntOfLength(const void *src, unsigned length) {
        assert(length >= 1 && length <= 8);
        int64_t result = 0;
        if (((int8_t*)src)[length-1] < 0)
            result = -1;
        memcpy(&result, src, length);
        return _decLittle64(result);
    }

    size_t PutIntOfLength(void *buf, int64_t n, bool isUnsigned) {
        int64_t littlen = _encLittle64(n);
        memcpy(buf, &littlen, 8);
        size_t size;
        uint8_t trim = (n >= 0 || isUnsigned) ? 0 : 0xFF;
        for (size = 8; size > 1; --size) {
            if (((uint8_t*)buf)[size-1] != trim)
                break;
        }
        return size;
    }

}