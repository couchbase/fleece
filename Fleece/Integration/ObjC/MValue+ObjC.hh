//
// MValue+ObjC.hh
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
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
@property (readonly, nonatomic) fleece::MCollection<id>* fl_collection;
@end


@interface FleeceArray ()

- (instancetype) initWithMValue: (fleece::MValue<id>*)mv
                       inParent: (fleece::MCollection<id>*)parent;

@end


@interface FleeceDict ()

- (instancetype) initWithMValue: (fleece::MValue<id>*)mv
                       inParent: (fleece::MCollection<id>*)parent;

@end

