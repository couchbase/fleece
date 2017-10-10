//
//  MValue+ObjC.m
//  Fleece
//
//  Created by Jens Alfke on 10/9/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "MValue+ObjC.hh"
#include "PlatformCompat.hh"


@implementation NSObject (FleeceMutable)
- (fleeceapi::MCollection<id>*) fl_collection {
    return nullptr;
}
@end


namespace fleeceapi {

    // These are the three MValue methods that have to be implemented in any specialization,
    // here specialized for <id>.

    template<>
    id MValue<id>::toNative(MValue *mv, MCollection<id> *parent, bool &cacheIt) {
        switch (mv->value().type()) {
            case kFLArray:
                cacheIt = true;
                return [[FleeceArray alloc] initWithMValue: mv
                                                  inParent: parent
                                                 isMutable: parent->mutableContainers()];
            case kFLDict:
                cacheIt = true;
                return [[FleeceDict alloc] initWithMValue: mv
                                                 inParent: parent
                                                isMutable: parent->mutableContainers()];
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

