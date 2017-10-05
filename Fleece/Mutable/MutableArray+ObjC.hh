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
- (instancetype) initWithMValue: (fleece::MValue<id>*)mv
                       inParent: (fleece::MCollection<id>*)parent
                      isMutable: (bool)isMutable;
@end

