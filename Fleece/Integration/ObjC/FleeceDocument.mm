//
// FleeceDocument.m
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#import "FleeceDocument.h"
#import "MArray+ObjC.h"
#import "MDict+ObjC.h"
#import "MRoot.hh"

using namespace fleece;


@implementation FleeceDocument
{
    MRoot<id>* _root;
}


- (instancetype) initWithFleeceData: (fleece::alloc_slice)fleeceData
                  mutableContainers: (bool)mutableContainers
{
    self = [super init];
    if (self) {
        _root = new MRoot<id>(fleeceData, mutableContainers);
        if (!_root)
            self = nil;
    }
    return self;
}


- (void)dealloc
{
    delete _root;
}


- (id) rootObject {
    return _root->asNative();
}


+ (id) objectFromFleeceSlice: (fleece::alloc_slice)fleeceData
           mutableContainers: (bool)mutableContainers
{
    MRoot<id> root(fleeceData, mutableContainers);
    return root.asNative();
}


+ (id) objectFromFleeceData: (NSData*)fleeceData
          mutableContainers: (bool)mutableContainers
{
    return [self objectFromFleeceSlice: alloc_slice(fleeceData)
                     mutableContainers: mutableContainers];
}

@end
