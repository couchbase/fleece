//
// slice.hh
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

#pragma once

#include "Base.h"
#include "FLSlice.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <memory>
#include <assert.h>

#ifdef __OBJC__
#import <Foundation/NSData.h>
#import <Foundation/NSString.h>
@class NSMapTable;
#endif

#ifdef __APPLE__
struct __CFString;
struct __CFData;
#endif

#if __cplusplus >= 201700L
    // `constexpr17` is for uses of `constexpr` that are valid in C++17 but not earlier.
    #define constexpr17 constexpr

    #if __has_include(<string_view>)
        #include <string_view>
        #define STRING_VIEW std::string_view
    #else
        #include <experimental/string_view>
        #define STRING_VIEW std::experimental::string_view
    #endif
#else
    #define constexpr17
#endif


// Utility for using slice with printf-style formatting.
// Use "%.*" in the format string; then for the corresponding argument put FMTSLICE(theslice).
#define FMTSLICE(S)    (int)(S).size, (const char*)(S).buf


namespace fleece {
    struct slice;
    struct alloc_slice;
    struct nullslice_t;

#ifdef STRING_VIEW
    using string_view = STRING_VIEW; // create typedef with correct namespace
    #undef STRING_VIEW
    #define SLICE_SUPPORTS_STRING_VIEW
#endif

#ifdef __APPLE__
    using CFStringRef = const struct ::__CFString *;
    using CFDataRef   = const struct ::__CFData *;
#endif
    
    /** Adds a byte offset to a pointer. */
    template <typename T>
    inline const T* offsetby(const T *t, ptrdiff_t offset) {
        return (const T*)((uint8_t*)t + offset);
    }

    template <typename T>
    inline T* offsetby(T *t, ptrdiff_t offset) {
        return (T*)((uint8_t*)t + offset);
    }


    /** A simple range of memory. No ownership implied.
        The memory range (buf and size) is immutable; the `slice` subclass changes this. */
    struct pure_slice {
        const void* const buf;
        size_t      const size;

        constexpr pure_slice()                           :buf(nullptr), size(0) {}
        constexpr pure_slice(const void* b, size_t s)    :buf(b), size(s) {}

        constexpr17 pure_slice(const char* str)          :pure_slice(str, _strlen(str)) {}
        pure_slice(const std::string& str)               :pure_slice(&str[0], str.length()) {}
#ifdef SLICE_SUPPORTS_STRING_VIEW
        constexpr pure_slice(string_view str)            :pure_slice(str.data(), str.length()) {}
#endif

        explicit operator bool() const              {return buf != nullptr;}

        const void* offset(size_t o) const          {return (uint8_t*)buf + o;}
        size_t offsetOf(const void* ptr NONNULL) const {return (uint8_t*)ptr - (uint8_t*)buf;}
        const void* end() const                     {return offset(size);}

        inline slice upTo(const void* pos NONNULL) const PURE;
        inline slice from(const void* pos NONNULL) const PURE;
        inline slice upTo(size_t offset) const PURE;
        inline slice from(size_t offset) const PURE;

        const uint8_t& operator[](size_t i) const   {return ((const uint8_t*)buf)[i];}
        inline slice operator()(size_t i, size_t n) const PURE;

        slice find(pure_slice target) const noexcept PURE;
        const uint8_t* findByte(uint8_t b) const    {return (const uint8_t*)::memchr(buf, b, size);}
        const uint8_t* findByteOrEnd(uint8_t byte) const noexcept PURE;
        const uint8_t* findAnyByteOf(pure_slice targetBytes) const noexcept PURE;
        const uint8_t* findByteNotIn(pure_slice targetBytes) const noexcept PURE;

        int compare(pure_slice) const noexcept PURE;
        int caseEquivalentCompare(pure_slice) const noexcept PURE;
        bool caseEquivalent(pure_slice) const noexcept PURE;
        bool operator==(const pure_slice &s) const       {return size==s.size &&
                                                                 memcmp(buf, s.buf, size) == 0;}
        bool operator!=(const pure_slice &s) const       {return !(*this == s);}
        bool operator<(pure_slice s) const               {return compare(s) < 0;}
        bool operator>(pure_slice s) const               {return compare(s) > 0;}

        bool hasPrefix(pure_slice) const noexcept PURE;
        bool hasSuffix(pure_slice) const noexcept PURE;
        bool hasPrefix(uint8_t b) const noexcept        {return size > 0 && (*this)[0] == b;}
        bool hasSuffix(uint8_t b) const noexcept        {return size > 0 && (*this)[size-1] == b;}
        const void* containsBytes(pure_slice bytes) const noexcept PURE;

        bool containsAddress(const void *addr) const noexcept PURE;
        bool containsAddressRange(pure_slice) const noexcept PURE;

        slice copy() const;

        explicit operator std::string() const       {return std::string((const char*)buf, size);}
        std::string asString() const                {return (std::string)*this;}
        std::string hexString() const;
        std::string base64String() const;

#ifdef SLICE_SUPPORTS_STRING_VIEW
        operator string_view() const                {return string_view((const char*)buf, size);}
#endif

        /** Copies into a C string buffer of the given size. Result is always NUL-terminated and
            will not overflow the buffer. Returns false if the slice was truncated. */
        bool toCString(char *buf, size_t bufSize) const noexcept;

        /** Decodes Base64 data from receiver into output. On success returns subrange of output
            where the decoded data is. If output is too small to hold all the decoded data, returns
            a null slice. */
        slice readBase64Into(pure_slice output) const noexcept;

        /** Decodes Base64 data from receiver into a new alloc_slice.
            On failure returns a null slice. */
        alloc_slice decodeBase64() const;

        #define hexCString() hexString().c_str()    // has to be a macro else dtor called too early
        #define cString() asString().c_str()        // has to be a macro else dtor called too early

        /** Computes a 32-bit FNV-1a hash of the slice's contents. (Not cryptographic!) */
        uint32_t hash() const noexcept PURE;

        static constexpr17 uint32_t hash(const char *str NONNULL) noexcept PURE {
            uint32_t h = 2166136261;
            while (*str)
                h = (h ^ uint8_t(*str++)) * 16777619;
            return h;
        }

        /** Raw memory allocation. Just like malloc but throws on failure. */
        static void* newBytes(size_t sz);
        template <typename T>
        static T* reallocBytes(T* bytes, size_t newSz);

#ifdef __APPLE__
        explicit pure_slice(CFDataRef data);
        CFStringRef createCFString() const;
        CFDataRef createCFData() const;
#ifdef __OBJC__
        explicit pure_slice(NSData* data)                    :pure_slice((__bridge CFDataRef)data) {}

        NSData* copiedNSData() const {
            return buf ? [[NSData alloc] initWithBytes: buf length: size] : nil;
        }

        /** Creates an NSData using initWithBytesNoCopy and freeWhenDone:NO.
            The data is not copied and does not belong to the NSData object, so make sure it
            remains valid for the lifespan of that object!. */
        NSData* uncopiedNSData() const {
            if (!buf)
                return nil;
            return [[NSData alloc] initWithBytesNoCopy: (void*)buf length: size freeWhenDone: NO];
        }

        NSString* asNSString() const                {return CFBridgingRelease(createCFString());}
        NSString* asNSString(NSMapTable *sharedStrings) const;
#endif
#endif
        
    protected:
        void setBuf(const void *b NONNULL)          {const_cast<const void*&>(buf) = b;}
        void setSize(size_t s)                      {const_cast<size_t&>(size) = s;}
        void set(const void *b, size_t s)           {const_cast<const void*&>(buf) = b;
                                                     setSize(s);}
        pure_slice& operator=(pure_slice s)         {set(s.buf, s.size); return *this;}

        // like strlen but can run at compile time
        static constexpr17 size_t _strlen(const char *str) {
            if (!str)
                return 0;
            auto c = str;
            while (*c) ++c;
            return c - str;
        }
    };


    /** A simple range of memory. No ownership implied.
        Unlike its parent class pure_slice, this supports operations that change buf or size. */
    struct slice : public pure_slice {
        constexpr slice()                           :pure_slice() {}
        constexpr slice(std::nullptr_t)             :pure_slice() {}
        inline constexpr slice(nullslice_t);
        constexpr slice(const void* b, size_t s)    :pure_slice(b, s) {}
        constexpr slice(const void* start NONNULL, const void* end NONNULL)
                                                    :slice(start, (uint8_t*)end-(uint8_t*)start){}
        inline constexpr slice(const alloc_slice&);

        slice(const std::string& str)               :pure_slice(str) {}
        constexpr17 slice(const char* str)          :pure_slice(str) {}
#ifdef SLICE_SUPPORTS_STRING_VIEW
        constexpr slice(string_view str)            :pure_slice(str) {}
#endif

        slice& operator= (alloc_slice&&) =delete;   // Disallowed: might lead to ptr to freed buf
        slice& operator= (std::nullptr_t) noexcept  {set(nullptr, 0); return *this;}
        inline slice& operator= (nullslice_t) noexcept;

        void setBuf(const void *b NONNULL)          {pure_slice::setBuf(b);}
        void setSize(size_t s)                      {pure_slice::setSize(s);}
        void shorten(size_t s)                      {assert(s <= size); setSize(s);}

        void setEnd(const void* e NONNULL)          {setSize((uint8_t*)e - (uint8_t*)buf);}
        void setStart(const void* s NONNULL) noexcept;
        void moveStart(ptrdiff_t delta)             {set(offsetby(buf, delta), size - delta);}
        bool checkedMoveStart(size_t delta)         {if (size<delta) return false;
                                                     else {moveStart(delta); return true;}}
        slice read(size_t nBytes) noexcept;
        slice readAtMost(size_t nBytes) noexcept;
        slice readToDelimiter(slice delim) noexcept;
        slice readToDelimiterOrEnd(slice delim) noexcept;
        slice readBytesInSet(slice set) noexcept;
        bool readInto(slice dst) noexcept;

        bool writeFrom(slice) noexcept;

        uint8_t readByte() noexcept;     // returns 0 if slice is empty
        uint8_t peekByte() const noexcept PURE;     // returns 0 if slice is empty
        bool writeByte(uint8_t) noexcept;
        uint64_t readDecimal() noexcept; // reads until it hits a non-digit or the end
        int64_t readSignedDecimal() noexcept;
        bool writeDecimal(uint64_t) noexcept;
        static unsigned sizeOfDecimal(uint64_t) noexcept;

        void free() noexcept;

        constexpr slice(const FLSlice &s)           :slice(s.buf, s.size) { }
        operator FLSlice () const                   {return {buf, size};}
        inline explicit operator FLSliceResult () const;

#ifdef __APPLE__
        explicit slice(CFDataRef data)                       :pure_slice(data) {}
#ifdef __OBJC__
        explicit slice(NSData* data)                         :pure_slice(data) {}
#endif
#endif
    };


    struct nullslice_t : public slice {
        constexpr nullslice_t()   :slice() {}
    };
    
    /** A null/empty slice. (You can also use `nullptr` for this purpose.) */
    constexpr nullslice_t nullslice;


    // Literal syntax for slices: "foo"_sl
    inline constexpr slice operator "" _sl (const char *str NONNULL, size_t length)
        {return slice(str, length);}



    /** A slice that owns a ref-counted block of memory. */
    struct alloc_slice : public pure_slice {
        constexpr alloc_slice()                             {}
        constexpr alloc_slice(std::nullptr_t)               {}
        constexpr alloc_slice(nullslice_t)                  {}
        explicit alloc_slice(size_t s);
        explicit alloc_slice(pure_slice s);
        alloc_slice(const void* b, size_t s)
            :alloc_slice(slice(b, s))                       {}
        alloc_slice(const void* start NONNULL, const void* end NONNULL)
            :alloc_slice(slice(start, end))                 {}
        explicit alloc_slice(const char *str)               :alloc_slice(slice(str)) {}
        explicit alloc_slice(const std::string &str)        :alloc_slice(slice(str)) {}
        explicit alloc_slice(FLSlice s)     :alloc_slice(pure_slice{s.buf, s.size}) { }

        alloc_slice(FLHeapSlice s)     // FLHeapSlice is known to be an alloc_slice
        :pure_slice(s.buf, s.size)
        {
            retain();
        }

        alloc_slice(FLSliceResult &&sr)
        :pure_slice(sr.buf, sr.size)
        {
            sr.buf = nullptr;
            sr.size = 0;
        }

        alloc_slice(const FLSliceResult &sr)
        :pure_slice(sr.buf, sr.size)
        {
            retain();
        }

#ifdef SLICE_SUPPORTS_STRING_VIEW
        explicit alloc_slice(string_view str)               :alloc_slice(slice(str)) {}
#endif

        ~alloc_slice()                                      {if (buf) release();}
        alloc_slice(const alloc_slice&) noexcept;
        alloc_slice& operator=(const alloc_slice&) noexcept;

        alloc_slice(alloc_slice&& s) noexcept
        :pure_slice(s)
        { s.set(nullptr, 0); }

        alloc_slice& operator=(alloc_slice&& s) noexcept {
            release();
            assignFrom(s);
            s.set(nullptr, 0);
            return *this;
        }

        /** Creates an alloc_slice that has an extra null (0) byte immediately after the end of the
            data. This allows the contents of the alloc_slice to be used as a C string. */
        static alloc_slice nullPaddedString(pure_slice);

        alloc_slice& operator= (pure_slice s);
        alloc_slice& operator= (FLSlice s)                 {return operator=(slice(s.buf, s.size));}
        alloc_slice& operator= (FLHeapSlice) noexcept;
        alloc_slice& operator= (std::nullptr_t) noexcept    {reset(); return *this;}

        explicit operator bool() const                      {return buf != nullptr;}

        void reset() noexcept;
        void reset(size_t);
        void resize(size_t newSize);
        void append(pure_slice);

        // disambiguation:
        alloc_slice& operator=(const char *str NONNULL)     {*this = (slice)str; return *this;}
        alloc_slice& operator=(const std::string &str)      {*this = (slice)str; return *this;}

#ifdef SLICE_SUPPORTS_STRING_VIEW
        alloc_slice& operator=(string_view str)             {*this = (slice)str; return *this;}
#endif

        alloc_slice& retain() noexcept                      {FLSliceResult_Retain({buf,size}); return *this;}
        inline void release() noexcept                      {FLSliceResult_Release({buf,size});}

        static void retain(slice s) noexcept                {((alloc_slice*)&s)->retain();}
        static void release(slice s) noexcept               {((alloc_slice*)&s)->release();}

        operator FLSlice () const noexcept                  {return {buf, size};}
        operator FLHeapSlice () const noexcept              {return {buf, size};}
        explicit operator FLSliceResult () noexcept         {retain(); return {(void*)buf, size};}

        void shorten(size_t s)                          {assert(s <= size); pure_slice::setSize(s);}

#ifdef __APPLE__
        explicit alloc_slice(CFDataRef);
        explicit alloc_slice(CFStringRef);

        /** Creates a CFDataDataRef. The data is not copied: the CFDataRef points to the same
            bytes as this alloc_slice, which is retained until the CFDataRef is freed. */
        CFDataRef createCFData() const;

#ifdef __OBJC__
        explicit alloc_slice(NSData *data)             :alloc_slice((__bridge CFDataRef)data) { }

        /** Creates an NSData using initWithBytesNoCopy and a deallocator that releases this
            alloc_slice. The data is not copied and does not belong to the NSData object. */
        NSData* uncopiedNSData() const                 {return CFBridgingRelease(createCFData());}
#endif
#endif

    private:
        struct sharedBuffer;
        sharedBuffer* shared() noexcept;
        void assignFrom(pure_slice s)                       {set(s.buf, s.size);}

        friend struct ::FLSliceResult;
    };



    /** A slice whose `buf` may not be NULL. For use as a parameter type. */
    struct slice_NONNULL : public slice {
        constexpr slice_NONNULL(const void* b NONNULL, size_t s)    :slice(b, s) {}
        constexpr slice_NONNULL(slice s)                            :slice_NONNULL(s.buf, s.size) {}
        constexpr slice_NONNULL(FLSlice s)                          :slice_NONNULL(s.buf,s.size) {}
        constexpr17 slice_NONNULL(const char *str NONNULL)          :slice(str) {}
        slice_NONNULL(alloc_slice s)                                :slice_NONNULL(s.buf,s.size) {}
        slice_NONNULL(const std::string &str)               :slice_NONNULL(str.data(),str.size()) {}
#ifdef SLICE_SUPPORTS_STRING_VIEW
        slice_NONNULL(string_view str)                      :slice_NONNULL(str.data(),str.size()) {}
#endif
        slice_NONNULL(std::nullptr_t) =delete;
        slice_NONNULL(nullslice_t) =delete;
    };



#ifdef __APPLE__
    /** A slice holding the data of an NSString. It might point directly into the NSString, so
        don't modify or release the NSString while this is in scope. Or instead it might copy
        the data into a small internal buffer, or allocate it on the heap. */
    struct nsstring_slice : public slice {
        nsstring_slice(CFStringRef);
#ifdef __OBJC__
        nsstring_slice(NSString *str)   :nsstring_slice((__bridge CFStringRef)str) { }
#endif
        ~nsstring_slice();
    private:
        long getBytes(CFStringRef, long lengthInChars);
        char _local[127];
        bool _needsFree;
    };
#endif


    /** Functor class for hashing the contents of a slice (using the djb2 hash algorithm.)
        Suitable for use with std::unordered_map. */
    struct sliceHash {
        std::size_t operator() (pure_slice const& s) const {return s.hash();}
    };


    // Inlines that couldn't be implemented inside the class declaration due to forward references:
    inline slice pure_slice::upTo(const void* pos) const           {return slice(buf, pos);}
    inline slice pure_slice::from(const void* pos) const           {return slice(pos, end());}
    inline slice pure_slice::upTo(size_t off) const                {return slice(buf, off);}
    inline slice pure_slice::from(size_t off) const                {return slice(offset(off), end());}
    inline slice pure_slice::operator()(size_t i, size_t n) const  {return slice(offset(i), n);}

    inline constexpr slice::slice(nullslice_t)                     :pure_slice() {}
    inline slice& slice::operator= (nullslice_t) noexcept          {set(nullptr, 0); return *this;}
    inline constexpr slice::slice(const alloc_slice &s)            :pure_slice(s) { }
    inline slice::operator FLSliceResult () const {
        return FLSliceResult(alloc_slice(*this));
    }

}

namespace std {
    template<> struct hash<fleece::slice> {
        std::size_t operator() (fleece::pure_slice const& s) const {return s.hash();}
    };
    template<> struct hash<fleece::alloc_slice> {
        std::size_t operator() (fleece::pure_slice const& s) const {return s.hash();}
    };
}
