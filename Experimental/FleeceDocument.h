//
// FleeceDocument.h
//
// Copyright (c) 2015 Couchbase, Inc All rights reserved.
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

#import <Foundation/Foundation.h>


/** Objective-C bridge to Fleece. */
@interface FleeceDocument : NSObject

/** Returns an Objective-C object equivalent to the Fleece data. */
+ (id) objectWithFleeceData: (NSData*)fleece
                    trusted: (BOOL)trusted;

+ (id) objectWithFleeceBytes: (const void*)bytes
                      length: (size_t)length
                     trusted: (BOOL)trusted;

/** Converts an Objective-C object tree to Fleece data.
    Supported classes are the ones allowed by NSJSONSerialization, plus NSData. */
+ (NSData*) fleeceDataWithObject: (id)object
                           error: (NSError**)outError;

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
