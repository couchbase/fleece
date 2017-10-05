//
//  FleeceDocument.m
//  Fleece
//
//  Created by Jens Alfke on 10/4/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#import "FleeceDocument.h"
#import "MutableArray+ObjC.hh"
#import "MutableDict+ObjC.hh"

using namespace fleece;


@implementation FleeceDocument
{
    MRoot<id> _root;
}


- (instancetype) initWithFleeceData: (fleece::alloc_slice)fleeceData
                         sharedKeys: (FLSharedKeys)sharedKeys
                  mutableContainers: (bool)mutableContainers
{
    self = [super init];
    if (self) {
        _root = MRoot<id>(fleeceData, (SharedKeys*)sharedKeys, mutableContainers);
        if (!_root)
            self = nil;
    }
    return self;
}


- (id) rootObject {
    return _root.asNative();
}


+ (id) objectFromFleeceSlice: (fleece::alloc_slice)fleeceData
                  sharedKeys: (FLSharedKeys)sharedKeys
           mutableContainers: (bool)mutableContainers
{
    MRoot<id> root(fleeceData, (SharedKeys*)sharedKeys, mutableContainers);
    return root.asNative();
}


+ (id) objectFromFleeceData: (NSData*)fleeceData
                 sharedKeys: (FLSharedKeys)sharedKeys
          mutableContainers: (bool)mutableContainers
{
    return [self objectFromFleeceSlice: alloc_slice(fleeceData)
                            sharedKeys: sharedKeys
                     mutableContainers: mutableContainers];
}

@end
