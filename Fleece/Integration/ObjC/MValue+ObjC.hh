//
// MValue+ObjC.hh
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

#pragma once
#import <Foundation/Foundation.h>
#import "MArray+ObjC.h"
#import "MDict+ObjC.h"
#include "MCollection.hh"


#define UU __unsafe_unretained


// Defines an fl_collection property that returns null by default.
// FleeceArray and FleeceDictionary override this to return their MCollection instance.
// This is used by the implementation of MValue<id>::encodeNative(), below.
@interface NSObject (FleeceMutable)
@property (readonly, nonatomic) fleeceapi::MCollection<id>* fl_collection;
@end


@interface FleeceArray ()

- (instancetype) initWithMValue: (fleeceapi::MValue<id>*)mv
                       inParent: (fleeceapi::MCollection<id>*)parent;

@end


@interface FleeceDict ()

- (instancetype) initWithMValue: (fleeceapi::MValue<id>*)mv
                       inParent: (fleeceapi::MCollection<id>*)parent;

@end

