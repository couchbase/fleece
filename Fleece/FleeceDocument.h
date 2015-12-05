//
//  FleeceDocument.h
//  Fleece
//
//  Created by Jens Alfke on 12/4/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//

#import <Foundation/Foundation.h>


/** Objective-C bridge to Fleece. */
@interface FleeceDocument : NSObject

/** Returns an Objective-C object equivalent to the Fleece data. */
+ (id) objectWithFleeceData: (NSData*)fleece
                    trusted: (BOOL)trusted;

/** Converts an Objective-C object tree to Fleece data.
    Supported classes are the ones allowed by NSJSONSerialization, plus NSData. */
+ (NSData*) fleeceDataWithObject: (id)object;

@end


/** A dictionary subclass that's bridged to a Fleece 'dict' value. Created by FleeceDocument. */
@interface FleeceDictionary : NSDictionary

/** Equivalent to `[self objectForKey: key] != nil`, but faster because it doesn't have to
    translate the object from Fleece to Objective-C. */
- (BOOL)containsObjectForKey: (NSString*)key;

@end


/** A dictionary subclass that's bridged to a Fleece 'array' value. Created by FleeceDocument.*/
@interface FleeceArray : NSArray

@end
