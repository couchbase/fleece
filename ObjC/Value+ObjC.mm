//
// Value+ObjC.mm
//
// Copyright (c) 2015 Couchbase, Inc All rights reserved.
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

#import <Foundation/Foundation.h>
#include "FleeceImpl.hh"
#include "SharedKeys.hh"
#include "FleeceException.hh"


#define LOG_CACHED_STRINGS 0


// NSMapXXX C API isn't available in iOS
#if TARGET_OS_IPHONE
#define MapGet(STRINGS, KEY)           [(STRINGS) objectForKey: (__bridge id)(KEY)]
#define MapInsert(STRINGS, KEY, VALUE) [(STRINGS) setObject: (VALUE) \
                                                     forKey: (__bridge id)(KEY)]
#else
#define MapGet(STRINGS, KEY)           (__bridge NSString*)NSMapGet((STRINGS), (KEY))
#define MapInsert(STRINGS, KEY, VALUE) NSMapInsert((STRINGS), (KEY), (__bridge void*)(VALUE))
#endif


// Minimum size of NSString that will be allocated on the heap, as opposed to being a tagged ptr.
// See <https://www.mikeash.com/pyblog/friday-qa-2015-07-31-tagged-pointer-strings.html>
#if TARGET_RT_64_BIT
static const size_t kMinAllocedNSStringSize = 8;
#else
static const size_t kMinAllocedNSStringSize = 1;  // do tagged strings exist in 32-bit? Guessing no
#endif


namespace fleece {
    using namespace fleece::impl::internal;

    static NSString* convertString(pure_slice strSlice) {
        auto str = strSlice.asNSString();
        throwIf(!str, InvalidData, "Invalid UTF-8 in string");
        return str;
    }


    NSString* pure_slice::asNSString(NSMapTable *sharedStrings) const {
        if (sharedStrings == nil || size < std::max(kMinSharedStringSize,
                                                    kMinAllocedNSStringSize)
            || size > kMaxSharedStringSize)
            return convertString(*this);

        // Look up an existing shared string for this Value*:
        NSString* str = MapGet(sharedStrings, buf);
        if (!str) {
            str = convertString(*this);
            MapInsert(sharedStrings, buf, str);
    #if LOG_CACHED_STRINGS
            fprintf(stderr, "SHAREDSTRINGS[%p] %p --> %p %s\n",
                    sharedStrings, buf, str, str.UTF8String);
    #endif
        }
    #if LOG_CACHED_STRINGS
        else {
            fprintf(stderr, "SHAREDSTRINGS[%p] *Used* %p --> %s\n",
                    sharedStrings, buf, str.UTF8String);
        }
    #endif
        return str;
    }

}


namespace fleece { namespace impl {

    // Creates an NSMapTable that maps opaque pointers to Obj-C objects (NSStrings).
    NSMapTable* Value::createSharedStringsTable() noexcept {
        return [[NSMapTable alloc] initWithKeyOptions: NSPointerFunctionsOpaquePersonality |
                                                       NSPointerFunctionsOpaqueMemory
                                         valueOptions: NSPointerFunctionsObjectPersonality |
                                                       NSPointerFunctionsStrongMemory
                                             capacity: 8];
    }


    id Value::toNSObject(__unsafe_unretained NSMapTable *sharedStrings) const
    {
        switch (type()) {
            case kNull:
                return [NSNull null];
            case kBoolean:
                return @(asBool());
            case kNumber:
                // It's faster to use CFNumber, than NSNumber or @(n)
                if (isInteger()) {
                    int64_t i = asInt();
                    if (isUnsigned())
                        return @((uint64_t)i);  // CFNumber can't do unsigned long long!
                    else
                        return CFBridgingRelease(CFNumberCreate(nullptr, kCFNumberLongLongType,  &i));
                } else if (isDouble()) {
                    double d = asDouble();
                    return CFBridgingRelease(CFNumberCreate(nullptr, kCFNumberDoubleType,  &d));
                } else {
                    float f = asFloat();
                    return CFBridgingRelease(CFNumberCreate(nullptr, kCFNumberFloatType,  &f));
                }
            case kString:
                return asString().asNSString(sharedStrings);
            case kData:
                return asData().copiedNSData();
            case kArray: {
                auto iter = asArray()->begin();
                auto result = [[NSMutableArray alloc] initWithCapacity: iter.count()];
                for (; iter; ++iter) {
                    [result addObject: iter->toNSObject(sharedStrings)];
                }
                return result;
            }
            case kDict: {
                Dict::iterator iter(asDict());
                auto result = [[NSMutableDictionary alloc] initWithCapacity: iter.count()];
                for (; iter; ++iter) {
                    NSString* key = iter.keyToNSString(sharedStrings);
                    result[key] = iter.value()->toNSObject(sharedStrings);
                }
                return result;
            }
            default:
                FleeceException::_throw(UnknownValue, "illegal typecode in Value; corrupt data?");
        }
    }


    NSString* Dict::iterator::keyToNSString(__unsafe_unretained NSMapTable *sharedStrings) const
    {
        if (!key())
            return nil;
        NSString* keyStr = nil;
        if (key()->isInteger()) {
            // Decode int key using SharedKeys:
            auto sk = _sharedKeys ? _sharedKeys : findSharedKeys();
            if (sk) {
                auto encodedKey = (int)key()->asInt();
                keyStr = (__bridge NSString*)sk->platformStringForKey(encodedKey);
                if (!keyStr) {
                    slice strSlice = sk->decode(encodedKey);
                    if (strSlice) {
                        keyStr = convertString(strSlice);
                        sk->setPlatformStringForKey(encodedKey,
                                                    (__bridge CFStringRef)keyStr);
#if LOG_CACHED_STRINGS
                        fprintf(stderr, "SHAREDKEY[%p] %d --> %s\n", sk, encodedKey, keyStr.UTF8String);
#endif
                    }
                }
#if LOG_CACHED_STRINGS
                else {
                    fprintf(stderr, "SHAREDKEY[%p] *Used* %d --> %s\n", sk, encodedKey, keyStr.UTF8String);
                }
#endif
            }
        }
        if (!keyStr)
            keyStr = key()->toNSObject(sharedStrings);
        return keyStr;
    }


} }
