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

#include "PlatformCompat.hh"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <memory>
#include <assert.h>

#ifdef __APPLE__
#import <CoreFoundation/CFString.h>
#ifdef __OBJC__
#import <Foundation/NSData.h>
#import <Foundation/NSString.h>
@class NSMapTable;
#endif
#endif

struct FLSlice; struct FLSliceResult;

namespace fleece {
    struct slice;
    struct alloc_slice;

    
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

        explicit operator bool() const              {return buf != nullptr;}

        const void* offset(size_t o) const          {return (uint8_t*)buf + o;}
        size_t offsetOf(const void* ptr NONNULL) const {return (uint8_t*)ptr - (uint8_t*)buf;}
        const void* end() const                     {return offset(size);}

        inline slice upTo(const void* pos NONNULL);
        inline slice from(const void* pos NONNULL);
        inline slice upTo(size_t offset);
        inline slice from(size_t offset);

        const uint8_t& operator[](size_t i) const   {return ((const uint8_t*)buf)[i];}
        inline slice operator()(size_t i, size_t n) const;

        slice find(pure_slice target) const;
        const uint8_t* findByte(uint8_t b) const    {return (const uint8_t*)::memchr(buf, b, size);}
        const uint8_t* findByteOrEnd(uint8_t byte) const;
        const uint8_t* findAnyByteOf(pure_slice targetBytes) const;
        const uint8_t* findByteNotIn(pure_slice targetBytes) const;

        int compare(pure_slice) const noexcept;
        bool caseEquivalent(pure_slice) const noexcept;
        bool operator==(const pure_slice &s) const       {return size==s.size &&
                                                                 memcmp(buf, s.buf, size) == 0;}
        bool operator!=(const pure_slice &s) const       {return !(*this == s);}
        bool operator<(pure_slice s) const               {return compare(s) < 0;}
        bool operator>(pure_slice s) const               {return compare(s) > 0;}

        bool hasPrefix(pure_slice) const noexcept;
        bool hasSuffix(pure_slice) const noexcept;

        slice copy() const;

        explicit operator std::string() const;
        std::string asString() const                {return (std::string)*this;}
        std::string hexString() const;
        std::string base64String() const;

        /** Decodes Base64 data from receiver into output. On success returns subrange of output
            where the decoded data is. If output is too small to hold all the decoded data, returns
            a null slice. */
        slice readBase64Into(pure_slice output) const;

        #define hexCString() hexString().c_str()    // has to be a macro else dtor called too early
        #define cString() asString().c_str()        // has to be a macro else dtor called too early

        /** djb2 hash algorithm */
        uint32_t hash() const {
            uint32_t h = 5381;
            for (size_t i = 0; i < size; i++)
                h = (h<<5) + h + (*this)[i];
            return h;
        }

        /** Raw memory allocation. Just like malloc but throws on failure. */
        static void* newBytes(size_t sz);
        template <typename T>
        static T* reallocBytes(T* bytes, size_t newSz);

#ifdef __APPLE__
        CFStringRef createCFString() const {
            if (!buf)
                return nullptr;
            return CFStringCreateWithBytes(nullptr, (const uint8_t*)buf, size,
                                           kCFStringEncodingUTF8, false);
        }
#ifdef __OBJC__
        pure_slice(NSData* data)
        :pure_slice(data.bytes, data.length) {}

        NSData* copiedNSData() const {
            return buf ? [[NSData alloc] initWithBytes: buf length: size] : nil;
        }

        /** Creates an NSData using initWithBytesNoCopy and freeWhenDone:NO.
            The data is not copied and does not belong to the NSData object. */
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
    };


    /** A simple range of memory. No ownership implied.
        Unlike its parent class pure_slice, this supports operations that change buf or size. */
    struct slice : public pure_slice {
        constexpr slice()                           :pure_slice() {}
        constexpr slice(const void* b, size_t s)    :pure_slice(b, s) {}
        slice(const void* start NONNULL, const void* end NONNULL)
                                                    :slice(start, (uint8_t*)end-(uint8_t*)start){}
        inline slice(const alloc_slice&);
        slice(const std::string& str)               :slice(&str[0], str.length()) {}
        explicit slice(const char* str)             :slice(str, str ? strlen(str) : 0) {}

        slice& operator=(pure_slice s)              {set(s.buf, s.size); return *this;}
        slice& operator= (alloc_slice&&) =delete;   // Disallowed: might lead to ptr to freed buf

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
        bool readInto(slice dst) noexcept;

        bool writeFrom(slice) noexcept;

        uint8_t readByte() noexcept;     // returns 0 if slice is empty
        uint8_t peekByte() const noexcept;     // returns 0 if slice is empty
        bool writeByte(uint8_t) noexcept;
        uint64_t readDecimal() noexcept; // reads until it hits a non-digit or the end
        int64_t readSignedDecimal() noexcept;
        bool writeDecimal(uint64_t) noexcept;
        static unsigned sizeOfDecimal(uint64_t) noexcept;

        void free() noexcept;

        slice(const FLSlice&);
        operator FLSlice () const;
        explicit operator FLSliceResult () const;

#ifdef __OBJC__
        slice(NSData* data)                         :pure_slice(data) {}
#endif
    };

    
    /** A null/empty slice. */
    constexpr slice nullslice = slice();


    // Literal syntax for slices: "foo"_sl
    inline constexpr slice operator "" _sl (const char *str NONNULL, size_t length)
        {return slice(str, length);}



    /** A slice that owns a ref-counted block of memory. */
    struct alloc_slice : public pure_slice {
        alloc_slice()
            :pure_slice() {}
        explicit alloc_slice(size_t s);
        explicit alloc_slice(pure_slice s);
        alloc_slice(const void* b, size_t s)
            :alloc_slice(slice(b, s)) { }
        alloc_slice(const void* start NONNULL, const void* end NONNULL)
            :alloc_slice(slice(start, end)) {}
        explicit alloc_slice(const std::string &str)
            :alloc_slice(slice(str)) {}
        explicit alloc_slice(FLSlice);
        alloc_slice(FLSliceResult&&);

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

        alloc_slice& operator= (pure_slice s);

        explicit operator bool() const                      {return buf != nullptr;}

        void reset() noexcept;
        void reset(size_t);
        void resize(size_t newSize);
        void append(pure_slice);

        // disambiguation:
        alloc_slice& operator=(const char *str NONNULL)     {*this = (slice)str; return *this;}
        alloc_slice& operator=(const std::string &str)      {*this = (slice)str; return *this;}

        alloc_slice& retain() noexcept;
        void release() noexcept;

        static void release(slice s) noexcept               {((alloc_slice*)&s)->release();}

        operator FLSlice () const;
        explicit operator FLSliceResult ();

        void shorten(size_t s)                          {assert(s <= size); pure_slice::setSize(s);}

#ifdef __APPLE__
        explicit alloc_slice(CFStringRef);
#endif

    private:
        struct sharedBuffer;
        sharedBuffer* shared() noexcept;
        void assignFrom(pure_slice s)                       {set(s.buf, s.size);}

        friend struct ::FLSliceResult;
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
        CFIndex getBytes(CFStringRef, CFIndex lengthInChars);
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
    inline slice pure_slice::upTo(const void* pos)                 {return slice(buf, pos);}
    inline slice pure_slice::from(const void* pos)                 {return slice(pos, end());}
    inline slice pure_slice::upTo(size_t off)                      {return slice(buf, off);}
    inline slice pure_slice::from(size_t off)                      {return slice(offset(off), end());}
    inline slice pure_slice::operator()(size_t i, size_t n) const  {return slice(offset(i), n);}

    inline slice::slice(const alloc_slice &s)  :pure_slice(s) { }

}
