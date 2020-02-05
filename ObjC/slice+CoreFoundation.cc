//
// slice+CoreFoundation.cc
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

#if __APPLE__
#include <CoreFoundation/CFString.h>


namespace fleece {

    pure_slice::pure_slice(CFDataRef data) noexcept
    :buf(CFDataGetBytePtr(data))
    ,size(CFDataGetLength(data))
    { }

    CFStringRef pure_slice::createCFString() const {
        if (!buf)
            return nullptr;
        return CFStringCreateWithBytes(nullptr, (const uint8_t*)buf, size,
                                       kCFStringEncodingUTF8, false);
    }


    CFDataRef pure_slice::createCFData() const {
        if (!buf)
            return nullptr;
        return CFDataCreate(nullptr, (const UInt8*)buf, size);
    }


    alloc_slice::alloc_slice(CFDataRef data)
    :alloc_slice(slice(data))
    { }


    alloc_slice::alloc_slice(CFStringRef str) {
        CFIndex lengthInChars = CFStringGetLength(str);
        resize(CFStringGetMaximumSizeForEncoding(lengthInChars, kCFStringEncodingUTF8));
        CFIndex byteCount;
        auto nChars = CFStringGetBytes(str, CFRange{0, lengthInChars}, kCFStringEncodingUTF8, 0,
                                       false, (UInt8*)buf, size, &byteCount);
        if (nChars < lengthInChars)
            throw std::runtime_error("couldn't get CFString bytes");
        resize(byteCount);
    }


    static CFAllocatorRef SliceAllocator() {
        static CFAllocatorContext context = {
            .version = 0,
            .info = nullptr,
            .retain = [](const void *p) -> const void* { alloc_slice::retain(slice(p,1)); return p; },
            .release = [](const void *p) -> void        { alloc_slice::release(slice(p,1)); },
            .deallocate = [](void *p, void *info) -> void   { alloc_slice::release(slice(p,1)); },
        };
        static CFAllocatorRef kAllocator = CFAllocatorCreate(nullptr, &context);
        return kAllocator;
    }


    CFDataRef alloc_slice::createCFData() const {
        if (!buf)
            return nullptr;
        auto data = CFDataCreateWithBytesNoCopy(nullptr, (const UInt8*)buf, size, SliceAllocator());
        if (!data)
            throw std::bad_alloc();
        const_cast<alloc_slice*>(this)->retain();
        return data;
    }

    

    nsstring_slice::nsstring_slice(CFStringRef str)
    :_needsFree(false)
    {
        if (!str)
            return;
        // First try to use a direct pointer to the bytes:
        auto cstr = CFStringGetCStringPtr(str, kCFStringEncodingUTF8);
        if (cstr) {
            set(cstr, strlen(cstr));
            return;
        }

        CFIndex lengthInChars = CFStringGetLength(str);
        if (size_t(lengthInChars) <= sizeof(_local)) {
            // Next try to copy the UTF-8 into a smallish stack-based buffer:
            set(&_local, sizeof(_local));
            if (getBytes(str, lengthInChars))
                return;
        }

        // Otherwise malloc a buffer to copy the UTF-8 into:
        auto maxByteCount = CFStringGetMaximumSizeForEncoding(lengthInChars, kCFStringEncodingUTF8);
        set(newBytes(maxByteCount), maxByteCount);
        _needsFree = true;
        if (!getBytes(str, lengthInChars))
            throw std::runtime_error("couldn't get NSString bytes");
    }

    nsstring_slice::~nsstring_slice() {
        if (_needsFree)
            ::free((void*)buf);
    }

    CFIndex nsstring_slice::getBytes(CFStringRef str, CFIndex lengthInChars) {
        CFIndex byteCount;
        auto nChars = CFStringGetBytes(str, CFRange{0, lengthInChars}, kCFStringEncodingUTF8, 0,
                                       false, (UInt8*)buf, size, &byteCount);
        if (nChars < lengthInChars)
            return false;
        shorten(byteCount);
        return true;
    }

}

#endif //__APPLE__
