//
//  MValue+ObjC.hh
//  Fleece
//
//  Created by Jens Alfke on 10/9/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#import <Foundation/Foundation.h>
#import "MutableArray+ObjC.h"
#import "MutableDict+ObjC.h"
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
                       inParent: (fleeceapi::MCollection<id>*)parent
                      isMutable: (bool)isMutable;

@end


@interface FleeceDict ()

- (instancetype) initWithMValue: (fleeceapi::MValue<id>*)mv
                       inParent: (fleeceapi::MCollection<id>*)parent
                      isMutable: (bool)isMutable;

@end

