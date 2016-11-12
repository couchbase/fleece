//
//  slice.hh
//  Fleece
//
//  Created by Jens Alfke on 4/20/14.
//  Copyright (c) 2014-2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <memory>

#ifdef __OBJC__
#import <Foundation/NSData.h>
#import <Foundation/NSString.h>
#import <CoreFoundation/CFString.h>
#endif


namespace fleece {
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


    /** A simple range of memory. No ownership implied. */
    struct slice {
        const void* buf;
        size_t      size;

        constexpr slice()                           :buf(nullptr), size(0) {}
        constexpr slice(const void* b, size_t s)    :buf(b), size(s) {}
        slice(const void* start,
                        const void* end)          :buf(start), size((uint8_t*)end-(uint8_t*)start){}

        slice(const std::string& str)               :buf(&str[0]), size(str.length()) {}
        explicit slice(const char* str)             :buf(str), size(str ? strlen(str) : 0) {}

        slice& operator= (alloc_slice&&) =delete;   // Disallowed: might lead to ptr to freed buf

        explicit operator bool() const              {return buf != nullptr;}

        const void* offset(size_t o) const          {return (uint8_t*)buf + o;}
        size_t offsetOf(const void* ptr) const      {return (uint8_t*)ptr - (uint8_t*)buf;}
        const void* end() const                     {return offset(size);}
        void setEnd(const void* e)                  {size = (uint8_t*)e - (uint8_t*)buf;}
        void setStart(const void* s) noexcept;

        slice upTo(const void* pos)                 {return slice(buf, pos);}
        slice from(const void* pos)                 {return slice(pos, end());}

        const uint8_t& operator[](size_t i) const     {return ((const uint8_t*)buf)[i];}
        slice operator()(size_t i, unsigned n) const  {return slice(offset(i), n);}

        slice read(size_t nBytes) noexcept;
        slice readAtMost(size_t nBytes) noexcept;
        bool readInto(slice dst) noexcept;

        bool writeFrom(slice) noexcept;

        uint8_t readByte() noexcept;     // returns 0 if slice is empty
        uint8_t peekByte() noexcept;     // returns 0 if slice is empty
        bool writeByte(uint8_t) noexcept;
        uint64_t readDecimal() noexcept; // reads until it hits a non-digit or the end
        int64_t readSignedDecimal() noexcept;
        bool writeDecimal(uint64_t) noexcept;
        static unsigned sizeOfDecimal(uint64_t) noexcept;

        const uint8_t* findByte(uint8_t byte) const    {return (const uint8_t*)::memchr(buf, byte, size);}
        const uint8_t* findByteOrEnd(uint8_t byte) const;
        const uint8_t* findAnyByteOf(slice targetBytes);

        int compare(slice) const noexcept;
        bool operator==(const slice &s) const       {return size==s.size &&
                                                     memcmp(buf, s.buf, size) == 0;}
        bool operator!=(const slice &s) const       {return !(*this == s);}
        bool operator<(slice s) const               {return compare(s) < 0;}
        bool operator>(slice s) const               {return compare(s) > 0;}

        bool hasPrefix(slice) const noexcept;

        void moveStart(ptrdiff_t delta)             {buf = offsetby(buf, delta); size -= delta;}
        bool checkedMoveStart(size_t delta)         {if (size<delta) return false;
                                                     else {moveStart(delta); return true;}}

        slice copy() const;
        void free() noexcept;

        /** Raw memory allocation. Just like malloc but throws on failure. */
        static void* newBytes(size_t sz) {
            void* result = ::malloc(sz);
            if (!result) throw std::bad_alloc();
            return result;
        }

        template <typename T>
        static T* reallocBytes(T* bytes, size_t newSz) {
            T* newBytes = (T*)::realloc(bytes, newSz);
            if (!newBytes) throw std::bad_alloc();
            return newBytes;
        }

        explicit operator std::string() const;
        std::string asString() const                {return (std::string)*this;}
        std::string hexString() const;

        std::string base64String() const;

        /** Decodes Base64 data from receiver into output. On success returns subrange of output
            where the decoded data is. If output is too small to hold all the decoded data, returns
            a null slice. */
        slice readBase64Into(slice output) const;


        #define hexCString() hexString().c_str()    // has to be a macro else dtor called too early
        #define cString() asString().c_str()        // has to be a macro else dtor called too early

        /** djb2 hash algorithm */
        uint32_t hash() const {
            uint32_t h = 5381;
            for (size_t i = 0; i < size; i++)
                h = (h<<5) + h + (*this)[i];
            return h;
        }


#ifdef __OBJC__
        slice(NSData* data)
        :buf(data.bytes), size(data.length) {}

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

        /** Creates an NSData using initWithBytesNoCopy but with freeWhenDone:YES.
            The data is not copied but it now belongs to the NSData object. */
        NSData* convertToNSData() {
            if (!buf)
                return nil;
            return [[NSData alloc] initWithBytesNoCopy: (void*)buf length: size freeWhenDone: YES];
        }

        explicit operator NSString* () const {
            if (!buf)
                return nil;
            return CFBridgingRelease(CFStringCreateWithBytes(nullptr, (const uint8_t*)buf, size,
                                                             kCFStringEncodingUTF8, NO));
        }
#endif
    };

    
    /** A null/empty slice. */
    constexpr slice nullslice = slice();


    // Literal syntax for slices: "foo"_sl
    inline constexpr slice operator "" _sl (const char *str, size_t length)
        {return slice(str, length);}



    /** A slice that owns a ref-counted block of memory. */
    struct alloc_slice : public slice {
        alloc_slice()
            :slice() {}
        explicit alloc_slice(size_t s);
        explicit alloc_slice(slice s);
        alloc_slice(const void* b, size_t s)
            :alloc_slice(slice(b, s)) { }
        alloc_slice(const void* start, const void* end)
            :alloc_slice(slice(start, end)) {}
        explicit alloc_slice(const std::string &str)
            :alloc_slice(slice(str)) {}

        ~alloc_slice()                                      {if (buf) release();}
        alloc_slice(const alloc_slice&) noexcept;
        alloc_slice& operator=(const alloc_slice&) noexcept;

        alloc_slice(alloc_slice&& s) noexcept
        :slice(s)
        { s.buf = nullptr; }

        alloc_slice& operator=(alloc_slice&& s) noexcept {
            release();
            assignFrom(s);
            s.buf = nullptr;
            return *this;
        }

        explicit operator bool() const                      {return buf != nullptr;}

#ifdef __OBJC__
        NSData* convertToNSData();
#endif

        void reset() noexcept;
        void reset(size_t);
        void resize(size_t newSize);
        void append(slice);

        alloc_slice& operator=(slice);

        // disambiguation:
        alloc_slice& operator=(const std::string &str)      {*this = (slice)str; return *this;}

        slice retain() noexcept;
        void release() noexcept;

        static void release(slice s) noexcept               {((alloc_slice*)&s)->release();}

    private:
        struct sharedBuffer;
        sharedBuffer* shared() noexcept;
        void assignFrom(slice s)                            {buf = s.buf; size = s.size;}
    };



#ifdef __OBJC__
    /** A slice holding the data of an NSString. It might point directly into the NSString, so
        don't modify or release the NSString while this is in scope. Or instead it might copy
        the data into a small internal buffer, or allocate it on the heap. */
    struct nsstring_slice : public slice {
        nsstring_slice(NSString*);
        ~nsstring_slice();
    private:
        char _local[127];
        bool _needsFree;
    };
#endif


    /** Functor class for hashing the contents of a slice (using the djb2 hash algorithm.)
        Suitable for use with std::unordered_map. */
    struct sliceHash {
        std::size_t operator() (slice const& s) const {return s.hash();}
    };

}
