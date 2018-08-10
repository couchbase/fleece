//
// MValue+ObjC.m
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
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

#include "MValue+ObjC.hh"
#include "PlatformCompat.hh"


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

