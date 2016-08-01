//
//  slice.cc
//  Fleece
//
//  Created by Jens Alfke on 5/12/14.
//  Copyright (c) 2014 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "slice.hh"
#include <algorithm>
#include <math.h>
#include <stdlib.h>

namespace fleece {

    int slice::compare(slice b) const {
        // Optimized for speed
        if (this->size == b.size)
            return memcmp(this->buf, b.buf, this->size);
        else if (this->size < b.size)
            return memcmp(this->buf, b.buf, this->size) ?: -1;
        else
            return memcmp(this->buf, b.buf, b.size) ?: 1;
    }

    slice slice::read(size_t nBytes) {
        if (nBytes > size)
            return null;
        slice result(buf, nBytes);
        moveStart(nBytes);
        return result;
    }

    bool slice::readInto(slice dst) {
        if (dst.size > size)
            return false;
        ::memcpy((void*)dst.buf, buf, dst.size);
        moveStart(dst.size);
        return true;
    }

    bool slice::writeFrom(slice src) {
        if (src.size > size)
            return false;
        ::memcpy((void*)buf, src.buf, src.size);
        moveStart(src.size);
        return true;
    }

    uint8_t slice::readByte() {
        if (size == 0)
            return 0;
        uint8_t result = (*this)[0];
        moveStart(1);
        return result;
    }

    bool slice::writeByte(uint8_t n) {
        if (size == 0)
            return false;
        *((char*)buf) = n;
        moveStart(1);
        return true;
    }

    uint64_t slice::readDecimal() {
        uint64_t n = 0;
        while (size > 0 && isdigit(*(char*)buf)) {
            n = 10*n + (*(char*)buf - '0');
            moveStart(1);
        }
        return n;
    }

    bool slice::writeDecimal(uint64_t n) {
        // Optimized for speed
        size_t len;
        if (n < 10) {
            if (size < 1)
                return false;
            *((char*)buf) = '0' + (char)n;
            len = 1;
        } else {
            char temp[20]; // max length is 20 decimal digits
            char *dst = &temp[20];
            len = 0;
            do {
                *(--dst) = '0' + (n % 10);
                n /= 10;
                len++;
            } while (n > 0);
            if (size < len)
                return false;
            ::memcpy((void*)buf, dst, len);
        }
        moveStart(len);
        return true;
    }

    unsigned slice::sizeOfDecimal(uint64_t n) {
        if (n == 0)
            return 1;
        return 1 + (unsigned)::floor(::log10(n));
    }

    slice slice::copy() const {
        if (buf == NULL)
            return *this;
        void* copied = newBytes(size);
        ::memcpy(copied, buf, size);
        return slice(copied, size);
    }

    void slice::free() {
        ::free((void*)buf);
        buf = NULL;
        size = 0;
    }
    
    bool slice::hasPrefix(slice s) const {
        return s.size > 0 && size >= s.size && ::memcmp(buf, s.buf, s.size) == 0;
    }

    slice::operator std::string() const {
        return std::string((const char*)buf, size);
    }

    const slice slice::null;

    void* alloc_slice::alloc(const void* src, size_t s) {
        void* buf = newBytes(s);
        ::memcpy((void*)buf, src, s);
        return buf;
    }

    alloc_slice& alloc_slice::operator=(slice s) {
        s = s.copy();
        buf = s.buf;
        size = s.size;
        reset((char*)buf);
        return *this;
    }


    void alloc_slice::resize(size_t newSize) {
        if (newSize != size) {
            void* newBuf = slice::reallocBytes((void*)buf, newSize);
            if (newBuf == buf) {
                size = newSize;
            } else {
                dontFree();
                *this = alloc_slice::adopt(newBuf, newSize);
            }
        }
    }


    std::string slice::hexString() const {
        static const char kDigits[17] = "0123456789abcdef";
        std::string result;
        for (size_t i = 0; i < size; i++) {
            uint8_t byte = (*this)[(unsigned)i];
            result += kDigits[byte >> 4];
            result += kDigits[byte & 0xF];
        }
        return result;
    }

}
