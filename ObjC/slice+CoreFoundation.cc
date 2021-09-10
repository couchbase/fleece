//
// slice+CoreFoundation.cc
//
// Copyright 2014-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
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
            .retain = [](const void *p) -> const void* {
                if (p)
                    alloc_slice::retain(slice(p,1));
                return p;
            },
            .release = [](const void *p) -> void {
                if (p)
                    alloc_slice::release(slice(p,1));
            },
            .deallocate = [](void *p, void *info) -> void {
                if (p)
                    alloc_slice::release(slice(p,1));
            },
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
