//
//  slice.hh
//  Fleece
//
//  Created by Jens Alfke on 4/20/14.
//  Copyright (c) 2014 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#ifndef Fleece_slice_hh
#define Fleece_slice_hh

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <memory>

#ifdef __OBJC__
#import <Foundation/NSData.h>
#import <Foundation/NSString.h>
#endif

template <typename T>
const T* offsetby(const T *t, ptrdiff_t offset) {
    return (const T*)((uint8_t*)t + offset);
}


namespace fleece {

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

        const uint8_t& operator[](size_t i) const     {return ((const uint8_t*)buf)[i];}
        slice operator()(size_t i, unsigned n) const  {return slice(offset(i), n);}

        slice read(size_t nBytes);
        bool readInto(slice dst);

        bool writeFrom(slice);

        const void* findByte(uint8_t byte) const    {return ::memchr(buf, byte, size);}

        int compare(slice) const;
        bool operator==(const slice &s) const       {return compare(s)==0;}
        bool operator!=(const slice &s) const       {return compare(s)!=0;}
        bool operator<(slice s) const               {return compare(s) < 0;}
        bool operator>(slice s) const               {return compare(s) > 0;}

        void moveStart(ptrdiff_t delta)             {buf = offsetby(buf, delta); size -= delta;}
        bool checkedMoveStart(size_t delta)         {if (size<delta) return false;
                                                     else {moveStart(delta); return true;}}

        slice copy() const;
        void free();
        
        bool hasPrefix(slice) const;
        
        explicit operator std::string() const;
        std::string hexString() const;
        
#ifdef __OBJC__
        slice(NSData* data)                         :buf(data.bytes), size(data.length) {}

        explicit operator NSString*() const;

        NSData* copiedNSData() const;

        /** Creates an NSData using initWithBytesNoCopy and freeWhenDone:NO.
            The data is not copied and does not belong to the NSData object. */
        NSData* uncopiedNSData() const;

        /** Creates an NSData using initWithBytesNoCopy but with freeWhenDone:YES.
            The data is not copied but it now belongs to the NSData object. */
        NSData* convertToNSData();
#endif
    };

    /** An allocated range of memory. Constructors allocate, destructor frees. */
    struct alloc_slice : private std::shared_ptr<char>, public slice {
        alloc_slice()
            :std::shared_ptr<char>(NULL), slice() {}
        explicit alloc_slice(size_t s)
            :std::shared_ptr<char>((char*)malloc(s),::free), slice(get(),s) {}
        explicit alloc_slice(slice s)
            :std::shared_ptr<char>((char*)s.copy().buf,::free), slice(get(),s.size) {}
        alloc_slice(const void* b, size_t s)
            :std::shared_ptr<char>((char*)alloc(b,s),::free), slice(get(),s) {}
        alloc_slice(const void* start, const void* end)
            :std::shared_ptr<char>((char*)alloc(start,(uint8_t*)end-(uint8_t*)start),::free),
             slice(get(),(uint8_t*)end-(uint8_t*)start) {}
        alloc_slice(std::string str)
            :std::shared_ptr<char>((char*)alloc(&str[0], str.length()),::free), slice(get(), str.length()) {}

        static alloc_slice adopt(slice s)            {return alloc_slice((void*)s.buf,s.size,true);}
        static alloc_slice adopt(void* buf, size_t size) {return alloc_slice(buf,size,true);}

        alloc_slice& operator=(slice);

    private:
        static void* alloc(const void* src, size_t size);
        explicit alloc_slice(void* adoptBuf, size_t size, bool)
            :std::shared_ptr<char>((char*)adoptBuf,::free), slice(get(),size) {}
    };

#ifdef __OBJC__
    struct nsstring_slice : public slice {
        nsstring_slice(NSString*);
        ~nsstring_slice();
    private:
        char _local[127];
        bool _needsFree;
    };
#endif
}

#endif
