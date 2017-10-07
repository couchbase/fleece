//
//  FleeceDocument.h
//  Fleece
//
//  Created by Jens Alfke on 10/4/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#import <Foundation/Foundation.h>
#include "Fleece.h"
#ifdef __cplusplus
#include "slice.hh"
#endif


@interface FleeceDocument : NSObject

+ (id) objectFromFleeceData: (NSData*)fleeceData
                 sharedKeys: (FLSharedKeys)sharedKeys
          mutableContainers: (bool)mutableContainers;

#ifdef __cplusplus
+ (id) objectFromFleeceSlice: (fleece::alloc_slice)fleeceData
                  sharedKeys: (FLSharedKeys)sharedKeys
           mutableContainers: (bool)mutableContainers;
#endif

@property (readonly) id rootObject;

@end
