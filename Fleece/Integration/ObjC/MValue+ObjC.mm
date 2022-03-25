//
// MValue+ObjC.m
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "MValue+ObjC.hh"
#include "fleece/PlatformCompat.hh"


@implementation NSObject (FleeceMutable)
- (fleece::MCollection<id>*) fl_collection {
    return nullptr;
}
@end


namespace fleece {

    // These are the three MValue methods that have to be implemented in any specialization,
    // here specialized for <id>.

    template<>
    id MValue<id>::toNative(MValue *mv, MCollection<id> *parent, bool &cacheIt) {
        switch (mv->value().type()) {
            case kFLArray:
                cacheIt = true;
                return [[FleeceArray alloc] initWithMValue: mv
                                                  inParent: parent];
            case kFLDict:
                cacheIt = true;
                return [[FleeceDict alloc] initWithMValue: mv
                                                 inParent: parent];
            default:
                return mv->value().asNSObject();
        }
    }

    template<>
    MCollection<id>* MValue<id>::collectionFromNative(id native) {
        return [native fl_collection];
    }

    template<>
    void MValue<id>::encodeNative(Encoder &enc, id obj) {
        enc << obj;
    }

}

