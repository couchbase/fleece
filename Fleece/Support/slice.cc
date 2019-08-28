//
// slice.cc
//
// Copyright (c) 2014 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "fleece/slice.hh"
#include "encode.h"
#include "decode.h"
#include <algorithm>
#include <math.h>
#include <stdlib.h>
#include <atomic>
#include <string.h>
#include <stdio.h>
#ifdef _MSC_VER
#include "memmem.h"
#endif
#include "betterassert.hh"

namespace fleece {

    void slice::setStart(const void *s) noexcept {
        assert(s <= end());
        set(s, (uint8_t*)end() - (uint8_t*)s);
    }

    bool pure_slice::containsAddress(const void *addr) const noexcept {
        return addr >= buf && addr < end();
    } 

    int pure_slice::compare(pure_slice b) const noexcept {
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

    int pure_slice::caseEquivalentCompare(pure_slice b) const noexcept {
        size_t minSize = std::min(size, b.size);
        for (size_t i = 0; i < minSize; i++) {
            int cmp = tolower((*this)[i]) - tolower(b[i]);
            if (cmp != 0)
                return cmp;
        }
        return (int)size - (int)b.size;
    }

    bool pure_slice::caseEquivalent(pure_slice b) const noexcept {
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

    slice slice::readToDelimiter(slice delim) noexcept {
        slice found = find(delim);
        if (!found)
            return nullslice;
        slice result(buf, found.buf);
        setStart(found.end());
        return result;
    }

    slice slice::readBytesInSet(slice set) noexcept {
        const void *next = findByteNotIn(set);
        if (!next)
            next = end();
        slice result(buf, next);
        setStart(next);
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

    uint8_t slice::peekByte() const noexcept {
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

    slice pure_slice::find(pure_slice target) const {
        auto found = memmem(buf, size, target.buf, target.size);
        return {found, (found ? target.size : 0)};
    }

    const uint8_t* pure_slice::findByteOrEnd(uint8_t byte) const {
        auto result = findByte(byte);
        return result ? result : (const uint8_t*)end();
    }

    const uint8_t* pure_slice::findAnyByteOf(pure_slice targetBytes) const {
        const void* result = nullptr;
        for (size_t i = 0; i < targetBytes.size; ++i) {
            auto r = findByte(targetBytes[i]);
            if (r && (!result || r < result))
                result = r;
        }
        return (const uint8_t*)result;
    }

    const uint8_t* pure_slice::findByteNotIn(pure_slice targetBytes) const {
        for (auto c = (const uint8_t*)buf; c != end(); ++c) {
            if (!targetBytes.findByte(*c))
                return c;
        }
        return nullptr;
    }


    /** Raw memory allocation. Just like malloc but throws on failure. */
    void* pure_slice::newBytes(size_t sz) {
        void* result = ::malloc(sz);
        if (!result) throw std::bad_alloc();
        return result;
    }

    template <typename T>
    T* pure_slice::reallocBytes(T* bytes, size_t newSz) {
        T* newBytes = (T*)::realloc(bytes, newSz);
        if (!newBytes) throw std::bad_alloc();
        return newBytes;
    }

    slice pure_slice::copy() const {
        if (buf == nullptr)
            return nullslice;
        void* copied = newBytes(size);
        ::memcpy(copied, buf, size);
        return slice(copied, size);
    }

    void slice::free() noexcept {
        ::free((void*)buf);
        set(nullptr, 0);
    }
    
    bool pure_slice::hasPrefix(pure_slice s) const noexcept {
        return s.size > 0 && size >= s.size && ::memcmp(buf, s.buf, s.size) == 0;
    }

    bool pure_slice::hasSuffix(pure_slice s) const noexcept {
        return s.size > 0 && size >= s.size
            && ::memcmp(offsetby(buf, size - s.size), s.buf, s.size) == 0;
    }

    const void* pure_slice::containsBytes(pure_slice s) const noexcept {
        return memmem(buf, size, s.buf, s.size);
    }

    bool pure_slice::containsAddressRange(pure_slice s) const noexcept {
        return s.buf >= buf && s.end() <= end();
    }

    pure_slice::operator std::string() const {
        return std::string((const char*)buf, size);
    }


    std::string pure_slice::hexString() const {
        static const char kDigits[17] = "0123456789abcdef";
        std::string result;
        for (size_t i = 0; i < size; i++) {
            uint8_t byte = (*this)[(unsigned)i];
            result += kDigits[byte >> 4];
            result += kDigits[byte & 0xF];
        }
        return result;
    }


    bool pure_slice::toCString(char *str, size_t bufSize) {
        size_t n = std::min(size, bufSize-1);
        memcpy(str, buf, n);
        str[n] = 0;
        return n == size;
    }


    std::string pure_slice::base64String() const {
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


    slice pure_slice::readBase64Into(pure_slice output) const {
        size_t expectedLen = (size + 3) / 4 * 3;
        if (expectedLen > output.size)
            return nullslice;
        base64::decoder dec;
        size_t len = dec.decode(buf, size, (void*)output.buf);
        assert(len <= output.size);
        return slice(output.buf, len);
    }


    alloc_slice pure_slice::decodeBase64() const {
        size_t expectedLen = (size + 3) / 4 * 3;
        alloc_slice result(expectedLen);
        slice decoded = readBase64Into(result);
        if (decoded.size == 0)
            return {};
        assert(decoded.size <= expectedLen);
        result.resize(decoded.size);
        return result;
    }


#pragma mark - ALLOC_SLICE


#ifndef NDEBUG
#if FL_EMBEDDED
    static constexpr size_t kHeapAlignmentMask = 0x03;
#else
    static constexpr size_t kHeapAlignmentMask = 0x07;
#endif
    static inline bool isHeapAligned(const void *p) {
        return ((size_t)p & kHeapAlignmentMask) == 0;
    }
#endif


// FL_DETECT_COPIES enables a check for unnecessary copying of alloc_slice memory: situations
// where an alloc_slice gets downcast to a slice, which is then used to construct another
// alloc_slice. If you can fix the calling code to avoid the downcast, the alloc_slice
// constructor will just retain instead of copying.
// This mode is incompatible with sanitizer tools like Clang's Address Sanitizer or valgrind,
// since it peeks outside the bounds of the input slice, which is very likely to trigger
// warnings about buffer overruns.
#ifndef FL_DETECT_COPIES
#define FL_DETECT_COPIES 0
#endif


    // The heap-allocated buffer that an alloc_slice points to.
    // It's ref-counted; every alloc_slice manages retaining/releasing its sharedBuffer.
    struct alloc_slice::sharedBuffer {
        std::atomic<uint32_t> _refCount {1};
#if FL_DETECT_COPIES
        static constexpr uint32_t kMagic = 0xdecade55;
        uint32_t const _magic {kMagic};
#endif
        uint8_t _buf[4];

        inline sharedBuffer* retain() noexcept {
            assert(isHeapAligned(this));
            ++_refCount;
            return this;
        }

        inline void release() noexcept {
            assert(isHeapAligned(this));
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

        static inline slice newSlice(pure_slice s) {
            if (!s.buf)
                return nullslice;
#if FL_DETECT_COPIES
            // Warn if s appears to be the buffer of an existing alloc_slice:
            if (s.buf && isHeapAligned(s.buf)
                      && ((size_t)s.buf & 0xFFF) >= 4   // reading another VM page may crash
                      && ((const uint32_t*)s.buf)[-1] == kMagic) {
                fprintf(stderr, "$$$$$ Copying existing alloc_slice at {%p, %zu}\n", s.buf, s.size);
            } 
#endif
            auto sb = newBuffer(s.size);
            memcpy(&sb->_buf, s.buf, s.size);
            return {&sb->_buf, s.size};
        }

        static inline void operator delete(void* ptr) {
            assert(isHeapAligned(ptr));
            ::operator delete(ptr);
        }
    };


    alloc_slice::alloc_slice(size_t sz)
    :pure_slice(sharedBuffer::newSlice(sz))
    { }


    alloc_slice::alloc_slice(pure_slice s)
    :pure_slice(sharedBuffer::newSlice(s))
    { }

    inline alloc_slice::sharedBuffer* alloc_slice::shared() noexcept {
        return offsetby((sharedBuffer*)buf, -((long long)offsetof(sharedBuffer, _buf)));
    }

    alloc_slice& alloc_slice::retain() noexcept     {if (buf) shared()->retain(); return *this;}
    void alloc_slice::release() noexcept            {if (buf) shared()->release();}


    alloc_slice::alloc_slice(const alloc_slice& s) noexcept
    :pure_slice(s)
    {
        retain();
    }

    alloc_slice alloc_slice::nullPaddedString(pure_slice str) {
        // Leave a trailing null byte after the end, so it can be used as a C string
        alloc_slice a(str.size + 1);
        memcpy((char*)a.buf, str.buf, str.size);
        ((char*)a.buf)[str.size] = '\0';
        a.shorten(str.size);            // the null byte is not part of the slice
        return a;
    }


    alloc_slice& alloc_slice::operator=(const alloc_slice& s) noexcept {
        if (s.buf != buf) {
            release();
            assignFrom(s);
            retain();
        }
        return *this;
    }

    alloc_slice& alloc_slice::operator=(FLHeapSlice s) noexcept {
        if (s.buf != buf) {
            release();
            assignFrom({s.buf, s.size});
            retain();
        }
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

    
    alloc_slice& alloc_slice::operator=(pure_slice s) {
        slice newS = sharedBuffer::newSlice(s);
        release();
        assignFrom(newS);
        return *this;
    }


    void alloc_slice::resize(size_t newSize) {
        if (newSize == size) {
            return;
        } else if (buf == nullptr) {
            reset(newSize);
        } else {
            sharedBuffer* newBuf = sharedBuffer::newBuffer(newSize);
            memcpy(newBuf->_buf, buf, std::min(size, newSize));
            release();
            set(newBuf->_buf, newSize);
        }
    }


    void alloc_slice::append(pure_slice suffix) {
        if (buf)
            assert(!containsAddress(suffix.buf) && !containsAddress(suffix.end()));
        size_t oldSize = size;
        resize(oldSize + suffix.size);
        memcpy((void*)offset(oldSize), suffix.buf, suffix.size);
    }

}
