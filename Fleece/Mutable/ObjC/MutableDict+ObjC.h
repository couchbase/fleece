//
//  MutableDict+ObjC.h
//  Fleece
//
//  Created by Jens Alfke on 5/28/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include <Foundation/Foundation.h>


@interface FleeceDict : NSMutableDictionary

- (BOOL)containsObjectForKey:(NSString *)key;

@property (readonly, nonatomic) bool isMutated;

@end
