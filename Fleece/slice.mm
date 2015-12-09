//
//  slice.mm
//  CBForest
//
//  Created by Jens Alfke on 5/14/14.
//  Copyright (c) 2014 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "slice.hh"
#import <Foundation/Foundation.h>

namespace fleece {

    NSData* slice::copiedNSData() const {
        return buf ? [NSData dataWithBytes: buf length: size] : nil;
    }

    NSData* slice::uncopiedNSData() const {
        if (!buf)
            return nil;
        return [[NSData alloc] initWithBytesNoCopy: (void*)buf length: size freeWhenDone: NO];
    }

    NSData* slice::convertToNSData() {
        if (!buf)
            return nil;
        return [[NSData alloc] initWithBytesNoCopy: (void*)buf length: size freeWhenDone: YES];
    }


    slice::operator NSString*() const {
        if (!buf)
            return nil;
        return [[NSString alloc] initWithBytes: buf length: size encoding: NSUTF8StringEncoding];
    }


    nsstring_slice::nsstring_slice(__unsafe_unretained NSString* str)
    :_needsFree(false)
    {
        NSUInteger byteCount;
        if (str.length <= sizeof(_local)) {
            if (!str)
                return;
            // First try to copy the UTF-8 into a smallish stack-based buffer:
            NSRange remaining;
            BOOL ok = [str getBytes: _local maxLength: sizeof(_local) usedLength: &byteCount
                           encoding: NSUTF8StringEncoding options: 0
                              range: NSMakeRange(0, str.length) remainingRange: &remaining];
            if (ok && remaining.length == 0) {
                buf = &_local;
                size = byteCount;
                return;
            }
        }

        // Otherwise malloc a buffer to copy the UTF-8 into:
        NSUInteger maxByteCount = [str maximumLengthOfBytesUsingEncoding: NSUTF8StringEncoding];
        buf = ::malloc(maxByteCount);
        if (!buf)
            throw std::bad_alloc();
        _needsFree = true;
        __unused BOOL ok = [str getBytes: (void*)buf maxLength: maxByteCount usedLength: &byteCount
                                encoding: NSUTF8StringEncoding options: 0
                                   range: NSMakeRange(0, str.length) remainingRange: NULL];
        assert(ok);
        size = byteCount;
    }

    nsstring_slice::~nsstring_slice() {
        if (_needsFree)
            ::free((void*)buf);
    }

}
