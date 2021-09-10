//
// MDict+ObjC.h
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
#include <Foundation/Foundation.h>


@interface FleeceDict : NSMutableDictionary

- (BOOL)containsObjectForKey:(NSString *)key;

@property (readonly, nonatomic) bool isMutated;

@end
