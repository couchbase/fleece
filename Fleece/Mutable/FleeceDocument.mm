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
#import "MRoot.hh"

using namespace fleeceapi;


@implementation FleeceDocument
{
    MRoot<id>* _root;
}


- (instancetype) initWithFleeceData: (fleeceapi::alloc_slice)fleeceData
                         sharedKeys: (FLSharedKeys)sharedKeys
                  mutableContainers: (bool)mutableContainers
{
    self = [super init];
    if (self) {
        _root = new MRoot<id>(fleeceData, (FLSharedKeys)sharedKeys, mutableContainers);
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


+ (id) objectFromFleeceSlice: (fleeceapi::alloc_slice)fleeceData
                  sharedKeys: (FLSharedKeys)sharedKeys
           mutableContainers: (bool)mutableContainers
{
    MRoot<id> root(fleeceData, (FLSharedKeys)sharedKeys, mutableContainers);
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
