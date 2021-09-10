//
// slice+ObjC.mm
//
// Copyright 2021-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "fleece/slice.hh"
#include <Foundation/Foundation.h>

namespace fleece {

    pure_slice::pure_slice(NSData* data) noexcept
    :pure_slice((__bridge CFDataRef)data)
    { }


    NSData* pure_slice::copiedNSData() const {
        return buf ? [[NSData alloc] initWithBytes: buf length: size] : nil;
    }


    NSData* pure_slice::uncopiedNSData() const {
        if (!buf)
            return nil;
        return [[NSData alloc] initWithBytesNoCopy: (void*)buf length: size freeWhenDone: NO];
    }


    NSString* pure_slice::asNSString() const {
        return CFBridgingRelease(createCFString());
    }


    alloc_slice::alloc_slice(NSData *data)
    :alloc_slice((__bridge CFDataRef)data)
    { }


    NSData* alloc_slice::uncopiedNSData() const {
        return CFBridgingRelease(createCFData());
    }

}
