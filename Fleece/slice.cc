//
//  slice.cc
//  Fleece
//
//  Created by Jens Alfke on 5/12/14.
//  Copyright (c) 2014-2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "slice.hh"
#include "encode.h"
#include "decode.h"
#include "Fleece.h" // for FLSlice and FLSliceResult
#include <algorithm>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <atomic>
#ifdef _MSC_VER
#include "memmem.h"
#endif

namespace fleece {

    void slice::setStart(const void *s) noexcept {
        assert(s <= end());
        size = (uint8_t*)end() - (uint8_t*)s;
        buf = s;
    }

    int slice::compare(slice b) const noexcept {
        // Optimized for speed
        if (this->size == b.size)
            return memcmp(this->buf, b.buf, this->size);
        else if (this->size < b.size) {
            int result = memcmp(this->buf, b.buf, this->size);
            return result ? result : -1;
        } else {
            int result = memcmp(this->buf, b.buf, b.size);
            return result ? result : 1;
        }
    }

    bool slice::caseEquivalent(slice b) const noexcept {
        if (size != b.size)
            return false;
        for (size_t i = 0; i < size; i++)
            if (tolower((*this)[i]) != tolower(b[i]))
                return false;
        return true;
    }

    slice slice::read(size_t nBytes) noexcept {
        if (nBytes > size)
            return nullslice;
        slice result(buf, nBytes);
        moveStart(nBytes);
        return result;
    }

    slice slice::readAtMost(size_t nBytes) noexcept {
        nBytes = std::min(nBytes, size);
        slice result(buf, nBytes);
        moveStart(nBytes);
        return result;
    }

    bool slice::readInto(slice dst) noexcept {
        if (dst.size > size)
            return false;
        ::memcpy((void*)dst.buf, buf, dst.size);
        moveStart(dst.size);
        return true;
    }

    bool slice::writeFrom(slice src) noexcept {
        if (src.size > size)
            return false;
        ::memcpy((void*)buf, src.buf, src.size);
        moveStart(src.size);
        return true;
    }

    uint8_t slice::peekByte() noexcept {
        return (size > 0) ? (*this)[0] : 0;
    }

    uint8_t slice::readByte() noexcept {
        if (size == 0)
            return 0;
        uint8_t result = (*this)[0];
        moveStart(1);
        return result;
    }

    bool slice::writeByte(uint8_t n) noexcept {
        if (size == 0)
            return false;
        *((char*)buf) = n;
        moveStart(1);
        return true;
    }

    uint64_t slice::readDecimal() noexcept {
        uint64_t n = 0;
        while (size > 0 && isdigit(*(char*)buf)) {
            n = 10*n + (*(char*)buf - '0');
            moveStart(1);
        }
        return n;
    }

    int64_t slice::readSignedDecimal() noexcept {
        bool negative = (size > 0 && (*this)[0] == '-');
        if (negative)
            moveStart(1);
        uint64_t n = readDecimal();
        if (n > INT64_MAX)
            return 0;
        return negative ? -(int64_t)n : (int64_t)n;
    }

    bool slice::writeDecimal(uint64_t n) noexcept {
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

    unsigned slice::sizeOfDecimal(uint64_t n) noexcept {
        if (n == 0)
            return 1;
        return 1 + (unsigned)::floor(::log10(n));
    }

    slice slice::find(slice target) const {
        auto found = memmem(buf, size, target.buf, target.size);
        return {found, (found ? target.size : 0)};
    }

    const uint8_t* slice::findByteOrEnd(uint8_t byte) const {
        auto result = findByte(byte);
        return result ? result : (const uint8_t*)end();
    }

    const uint8_t* slice::findAnyByteOf(slice targetBytes) {
        const void* result = nullptr;
        for (size_t i = 0; i < targetBytes.size; ++i) {
            auto r = findByte(targetBytes[i]);
            if (r && (!result || r < result))
                result = r;
        }
        return (const uint8_t*)result;
    }

    slice slice::copy() const {
        if (buf == nullptr)
            return *this;
        void* copied = newBytes(size);
        ::memcpy(copied, buf, size);
        return slice(copied, size);
    }

    void slice::free() noexcept {
        ::free((void*)buf);
        buf = nullptr;
        size = 0;
    }
    
    bool slice::hasPrefix(slice s) const noexcept {
        return s.size > 0 && size >= s.size && ::memcmp(buf, s.buf, s.size) == 0;
    }

    slice::operator std::string() const {
        return std::string((const char*)buf, size);
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


    std::string slice::base64String() const {
        std::string str;
        size_t strLen = ((size + 2) / 3) * 4;
        str.resize(strLen);
        char *dst = &str[0];
        base64::encoder enc;
        enc.set_chars_per_line(0);
        size_t written = enc.encode(buf, size, dst);
        written += enc.encode_end(dst + written);
        assert(written == strLen);
        (void)written;  // avoid compiler warning in release build when 'assert' is a no-op
        return str;
    }


    slice slice::readBase64Into(slice output) const {
        size_t expectedLen = (size + 3) / 4 * 3;
        if (expectedLen > output.size)
            return nullslice;
        base64::decoder dec;
        size_t len = dec.decode(buf, size, (void*)output.buf);
        assert(len <= output.size);
        return slice(output.buf, len);
    }


    slice::slice(FLSlice s)                 :buf(s.buf), size(s.size) { }
    slice::operator FLSlice () const        {return {buf, size};}


#pragma mark - ALLOC_SLICE


    struct alloc_slice::sharedBuffer {
        std::atomic<uint32_t> _refCount {1};
        uint8_t _buf[4];

        inline sharedBuffer* retain() noexcept {
            ++_refCount;
            return this;
        }

        inline void release() noexcept {
            if (--_refCount == 0)
                delete this;
        }

        static inline void* operator new(size_t basicSize, size_t bufferSize) {
            return ::operator new(basicSize - sizeof(sharedBuffer::_buf) + bufferSize);
        }

        static inline sharedBuffer* newBuffer(size_t size) {
            return new(size) sharedBuffer;
        }

        static inline slice newSlice(size_t size) {
            return {&newBuffer(size)->_buf, size};
        }

        inline sharedBuffer* realloc(size_t newSize) {
            return slice::reallocBytes(this, offsetof(sharedBuffer, _buf) + newSize);
        }

        static inline void operator delete(void* ptr) {
            ::operator delete(ptr);
        }
    };


    alloc_slice::alloc_slice(size_t sz)
    :slice(sharedBuffer::newSlice(sz))
    { }


    alloc_slice::alloc_slice(slice s)
    :slice(s.buf ? sharedBuffer::newSlice(s.size) : nullslice)
    {
        if (s.buf)
            memcpy((void*)buf, s.buf, size);
    }

    alloc_slice::alloc_slice(FLSlice s)
    :alloc_slice(s.buf, s.size)
    { }

    alloc_slice::alloc_slice(FLSliceResult &&sr)
    :slice(sr.buf, sr.size)
    {
        sr.buf = nullptr;
        sr.size = 0;
    }



    inline alloc_slice::sharedBuffer* alloc_slice::shared() noexcept {
        return offsetby((sharedBuffer*)buf, -offsetof(sharedBuffer, _buf));
    }

    slice alloc_slice::retain() noexcept      {if (buf) shared()->retain(); return *this;}
    void alloc_slice::release() noexcept      {if (buf) shared()->release();}


    alloc_slice::alloc_slice(const alloc_slice& s) noexcept
    :slice(s)
    {
        retain();
    }

    alloc_slice& alloc_slice::operator=(const alloc_slice& s) noexcept {
        const_cast<alloc_slice&>(s).retain();
        release();
        assignFrom(s);
        return *this;
    }


    void alloc_slice::reset() noexcept {
        release();
        assignFrom(nullslice);
    }

    void alloc_slice::reset(size_t sz) {
        release();
        assignFrom(sharedBuffer::newSlice(sz));
    }

    
    alloc_slice& alloc_slice::operator=(slice s) {
        if (s.buf) {
            bool noop = (s.buf == buf);
            reset(s.size);
            if (!noop)
                memcpy((void*)buf, s.buf, size);
        } else {
            reset();
        }
        return *this;
    }


    void alloc_slice::resize(size_t newSize) {
        if (newSize == size) {
            return;
        } else if (buf == nullptr) {
            reset(newSize);
        } else {
            sharedBuffer* newBuf;
            if (shared()->_refCount == 1) {
                newBuf = shared()->realloc(newSize);
            } else {
                newBuf = sharedBuffer::newBuffer(newSize);
                memcpy(newBuf->_buf, buf, std::min(size, newSize));
                release();
            }
            buf = newBuf->_buf;
            size = newSize;
        }
    }


    void alloc_slice::append(slice suffix) {
        size_t oldSize = size;
        resize(oldSize + suffix.size);
        memcpy((void*)offset(oldSize), suffix.buf, suffix.size);
    }


}
