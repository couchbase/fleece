//
// FleeceDocument.m
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
