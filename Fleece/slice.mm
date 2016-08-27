//
//  slice.mm
//  Couchbase Lite Core
//
//  Created by Jens Alfke on 5/14/14.
//  Copyright (c) 2014-2016 Couchbase. All rights reserved.
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

    nsstring_slice::nsstring_slice(__unsafe_unretained NSString* str)
    :_needsFree(false)
    {
        if (!str)
            return;
        // First try to use a direct pointer to the bytes:
        auto cstr = CFStringGetCStringPtr((__bridge CFStringRef)str, kCFStringEncodingUTF8);
        if (cstr) {
            size = strlen(cstr);
            buf = cstr;
            return;
        }

        NSUInteger byteCount;
        if (str.length <= sizeof(_local)) {
            // Next try to copy the UTF-8 into a smallish stack-based buffer:
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
        buf = newBytes(maxByteCount);
        _needsFree = true;
        BOOL ok = [str getBytes: (void*)buf maxLength: maxByteCount usedLength: &byteCount
                       encoding: NSUTF8StringEncoding options: 0
                          range: NSMakeRange(0, str.length) remainingRange: NULL];
        if (!ok)
            throw "couldn't get NSString bytes";
        size = byteCount;
    }

    nsstring_slice::~nsstring_slice() {
        if (_needsFree)
            ::free((void*)buf);
    }

}
