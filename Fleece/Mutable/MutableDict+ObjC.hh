//
//  MutableDict+ObjC.hh
//  Fleece
//
//  Created by Jens Alfke on 5/28/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "MDict.hh"
#include <Foundation/Foundation.h>


@interface FleeceDict : NSMutableDictionary

- (instancetype) initWithMValue: (fleece::MValue<id>*)mv
                       inParent: (fleece::MCollection<id>*)parent
                      isMutable: (bool)isMutable;

- (BOOL)containsObjectForKey:(NSString *)key;
@end
