//
//  MutableArray+ObjC.hh
//  Fleece
//
//  Created by Jens Alfke on 5/28/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "MArray.hh"
#include <Foundation/Foundation.h>


@interface FleeceArray : NSMutableArray
- (instancetype) initWithMValue: (fleeceapi::MValue<id>*)mv
                       inParent: (fleeceapi::MCollection<id>*)parent
                      isMutable: (bool)isMutable;

@property (readonly, nonatomic) bool isMutated;

@end

