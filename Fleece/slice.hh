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

        slice()                                   :buf(NULL), size(0) {}
        slice(const void* b, size_t s)            :buf(b), size(s) {}
        slice(const void* start, const void* end) :buf(start), size((uint8_t*)end-(uint8_t*)start){}
        slice(const std::string& str)             :buf(&str[0]), size(str.length()) {}

        explicit slice(const char* str)           :buf(str), size(str ? strlen(str) : 0) {}

        static const slice null;

        const void* offset(size_t o) const          {return (uint8_t*)buf + o;}
        size_t offsetOf(const void* ptr) const      {return (uint8_t*)ptr - (uint8_t*)buf;}
        const void* end() const                     {return offset(size);}
        void setEnd(const void* e)                  {size = (uint8_t*)e - (uint8_t*)buf;}

        slice upTo(const void* pos)                 {return slice(buf, pos);}
        slice from(const void* pos)                 {return slice(pos, end());}

        const uint8_t& operator[](size_t i) const     {return ((const uint8_t*)buf)[i];}
        slice operator()(size_t i, unsigned n) const  {return slice(offset(i), n);}

        slice read(size_t nBytes);
        slice readAtMost(size_t nBytes);
        bool readInto(slice dst);

        bool writeFrom(slice);

        uint8_t readByte();     // returns 0 if slice is empty
        bool writeByte(uint8_t);
        uint64_t readDecimal(); // reads until it hits a non-digit or the end
        bool writeDecimal(uint64_t);
        static unsigned sizeOfDecimal(uint64_t);

        const void* findByte(uint8_t byte) const    {return ::memchr(buf, byte, size);}

        int compare(slice) const;
        bool operator==(const slice &s) const       {return size==s.size &&
                                                     memcmp(buf, s.buf, size) == 0;}
        bool operator!=(const slice &s) const       {return !(*this == s);}
        bool operator<(slice s) const               {return compare(s) < 0;}
        bool operator>(slice s) const               {return compare(s) > 0;}

        void moveStart(ptrdiff_t delta)             {buf = offsetby(buf, delta); size -= delta;}
        bool checkedMoveStart(size_t delta)         {if (size<delta) return false;
                                                     else {moveStart(delta); return true;}}

        slice copy() const;
        void free();

        /** Raw memory allocation. Just like malloc but throws on failure. */
        static void* newBytes(size_t sz) {
            void* result = ::malloc(sz);
            if (!result) throw std::bad_alloc();
            return result;
        }

        template <typename T>
        static T* reallocBytes(T* bytes, size_t newSz) {
            void* newBytes = (T*)::realloc(bytes, newSz);
            if (!newBytes) throw std::bad_alloc();
            return newBytes;
        }

        bool hasPrefix(slice) const;
        
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
            return CFBridgingRelease(CFStringCreateWithBytes(NULL, (const uint8_t*)buf, size,
                                                             kCFStringEncodingUTF8, NO));
        }
#endif
    };



    /** An allocated range of memory. Constructors allocate, destructor frees. */
    struct alloc_slice : private std::shared_ptr<char>, public slice {
        alloc_slice()
            :std::shared_ptr<char>(NULL), slice() {}
        explicit alloc_slice(size_t s)
            :std::shared_ptr<char>((char*)newBytes(s), freer()), slice(get(),s) {}
        explicit alloc_slice(slice s)
            :std::shared_ptr<char>((char*)s.copy().buf, freer()), slice(get(),s.size) {}
        alloc_slice(const void* b, size_t s)
            :std::shared_ptr<char>((char*)alloc(b,s), freer()), slice(get(),s) {}
        alloc_slice(const void* start, const void* end)
            :std::shared_ptr<char>((char*)alloc(start,(uint8_t*)end-(uint8_t*)start), freer()),
             slice(get(),(uint8_t*)end-(uint8_t*)start) {}
        alloc_slice(std::string str)
            :std::shared_ptr<char>((char*)alloc(&str[0], str.length()), freer()), slice(get(), str.length()) {}

        static alloc_slice adopt(slice s)            {return alloc_slice((void*)s.buf,s.size,true);}
        static alloc_slice adopt(void* buf, size_t size) {return alloc_slice(buf,size,true);}

        /** Prevents the memory from being freed after the last alloc_slice goes away.
            Use this is something else (like an NSData) takes ownership of the heap block. */
        slice dontFree()             {if (buf) std::get_deleter<freer>(*this)->detach();
                                      return *this;}
#ifdef __OBJC__
        NSData* convertToNSData()   {dontFree(); return slice::convertToNSData();}
#endif

        void resize(size_t newSize);

        alloc_slice& operator=(slice);

        // disambiguation:
        alloc_slice& operator=(const std::string &str)      {*this = (slice)str; return *this;}

        class freer {
        public:
            freer()                     :_detached(false){}
            void detach()               {_detached = true;}
            void operator()(char* ptr)  {if (!_detached) ::free(ptr);}
        private:
            bool _detached;
        };


    private:
        static void* alloc(const void* src, size_t size);
        explicit alloc_slice(void* adoptBuf, size_t size, bool)     // called by adopt()
            :std::shared_ptr<char>((char*)adoptBuf, freer()), slice(get(),size) {}
        void reset(slice);
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
