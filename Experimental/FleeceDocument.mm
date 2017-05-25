//
//  FleeceDocument.mm
//  Fleece
//
//  Created by Jens Alfke on 12/4/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#import "FleeceDocument.h"
#import "Value.hh"
#import "Array.hh"
#import "Encoder.hh"
#import "Fleece.h"
using namespace fleece;

#define UU __unsafe_unretained


@interface FleeceDictionary ()
- (instancetype) initWithDict: (const fleece::Dict*)dictValue
                     document: (FleeceDocument*)document;
@property (readonly, nonatomic) const Dict* fleeceDict;
@end


@interface FleeceArray ()
- (instancetype) initWithArray: (const fleece::Array*)arrayValue
                      document: (FleeceDocument*)document;
@property (readonly, nonatomic) const Array* fleeceArray;
@end


#pragma mark - DOCUMENT:


// FleeceDocument is instantiated to hold onto the encoded data and the shared strings,
// but clients never see it; the public objects (FleeceDictionary, FleeceArray) hold references.
@implementation FleeceDocument
{
    NSData* _fleeceData;
    NSMapTable* _sharedStrings;
    id _root;
}

- (instancetype) initWithFleeceData: (UU NSData*)fleece
                            trusted: (BOOL)trusted
{
    NSParameterAssert(fleece != nil);
    self = [super init];
    if (self) {
        _fleeceData = [fleece copy];
        slice data(fleece);
        const Value *root = trusted ? Value::fromTrustedData(data) : Value::fromData(data);
        _sharedStrings = Value::createSharedStringsTable();
        _root = [self objectForValue: root];
        if (!_root)
            return nil;
    }
    return self;
}


- (id) objectForValue: (const Value*)v {
    if (!v)
        return nil;
    switch (v->type()) {
        case kArray:
            return [[FleeceArray alloc] initWithArray: v->asArray() document: self];
        case kDict:
            return [[FleeceDictionary alloc] initWithDict: v->asDict() document: self];
        default:
            return v->toNSObject(_sharedStrings);
    }
}


- (id) rootObject {
    return _root;
}


+ (id) objectWithFleeceData: (NSData*)fleece trusted: (BOOL)trusted {
    FleeceDocument *doc = [[self alloc] initWithFleeceData: fleece trusted: trusted];
    return doc.rootObject;
}


+ (id) objectWithFleeceBytes: (const void*)bytes length: (size_t)length trusted: (BOOL)trusted {
    slice s(bytes, length);
    const Value *root = trusted ? Value::fromTrustedData(s) : Value::fromData(s);
    if (!root)
        return nil;
    auto type = root->type();
    if (type == kArray || type == kDict)
        return [self objectWithFleeceData: [NSData dataWithBytes: bytes length: length]
                                  trusted: trusted];
    else
        return root->toNSObject();
}


+ (NSData*) fleeceDataWithObject: (id)object error: (NSError**)outError {
    NSParameterAssert(object != nil);
    FLEncoder enc = FLEncoder_New();
    FLEncoder_WriteNSObject(enc, object);
    NSData* data = FLEncoder_FinishWithNSData(enc, outError);
    FLEncoder_Free(enc);
    return data;
}


@end




#pragma mark - DICTIONARY:


@implementation FleeceDictionary
{
    const Dict* _dict;
    NSUInteger _count;
    FleeceDocument *_document;
}

@synthesize fleeceDict=_dict;


- (instancetype) initWithDict: (const Dict*)dictValue
                     document: (UU FleeceDocument*)document
{
    NSParameterAssert(dictValue);
    self = [super init];
    if (self) {
        _dict = dictValue;
        _count = _dict->count();
        _document = document;
    }
    return self;
}


- (id) copyWithZone:(NSZone *)zone {
    return self;
}


- (NSMutableDictionary*) mutableCopy {
    NSMutableDictionary* m = [[NSMutableDictionary alloc] initWithCapacity: _count];
    for (Dict::iterator iter(_dict); iter; ++iter) {
        id key = [_document objectForValue: iter.key()];
        m[key] = [_document objectForValue: iter.value()];
    }
    return m;
}


- (NSUInteger) count {
    return _count;
}


#pragma mark VALUE LOOKUP:


- (BOOL)containsObjectForKey:(UU NSString *)key {
    return _dict->get(key) != nullptr;
}


- (id) objectForKey: (UU id)key {
    if (![key isKindOfClass: [NSString class]])
        return nil;
    const Value* v = _dict->get((NSString*)key);
    return [_document objectForValue: v];
}


#pragma mark ENUMERATION:


- (void) forEachKey: (void(^)(NSString*, const Value*, BOOL*))block {
    for (Dict::iterator iter(_dict); iter; ++iter) {
        BOOL stop = NO;
        block([_document objectForValue: iter.key()], iter.value(), &stop);
        if (stop)
            break;
    }
}


- (NSArray*) allKeys {
    NSMutableArray* keys = [NSMutableArray arrayWithCapacity: _dict->count()];
    [self forEachKey: ^(NSString *key, const Value* v, BOOL* stop) {
        [keys addObject: key];
    }];
    return keys;
}


- (NSEnumerator *)keyEnumerator {
    return self.allKeys.objectEnumerator;
}


- (void) enumerateKeysAndObjectsUsingBlock: (void (^)(UU id key, UU id obj, BOOL *stop))block {
    [self forEachKey:^(NSString* key, const Value* v, BOOL* stop) {
        if (v) {
            id object = [_document objectForValue: v];
            if (object)
                block(key, object, stop);
        }
    }];
}


// Fast enumeration -- for(in) loops use this.
- (NSUInteger) countByEnumeratingWithState: (NSFastEnumerationState *)state
                                   objects: (id __unsafe_unretained [])stackBuf
                                     count: (NSUInteger)stackBufCount
{
    NSUInteger index = state->state;
    if (index == 0)
        state->mutationsPtr = &state->extra[0]; // this has to be pointed to something non-nullptr
    if (index >= _count)
        return 0;

    auto iter = _dict->begin();
    iter += (uint32_t)index;
    NSUInteger n = 0;
    for (; iter; ++iter) {
        NSString* key = [_document objectForValue: iter.key()];
        if (key) {
            CFAutorelease(CFBridgingRetain(key));
            stackBuf[n++] = key;
            if (n >= stackBufCount)
                break;
        }
    }

    state->itemsPtr = stackBuf;
    state->state += n;
    return n;
}


// This is what the %@ substitution calls.
- (NSString *)descriptionWithLocale:(id)locale indent:(NSUInteger)level {
    NSMutableString* desc = [@"{\n" mutableCopy];
    [self forEachKey: ^(NSString *key, const Value* v, BOOL *stop) {
        NSString* valStr = (NSString*)v->toJSON();
        [desc appendFormat: @"    \"%@\": %@,\n", key, valStr];
    }];
    [desc appendString: @"}"];
    return desc;
}


@end




#pragma mark - ARRAY:


@implementation FleeceArray
{
    const Array* _array;
    NSUInteger _count;
    FleeceDocument *_document;
}

@synthesize fleeceArray=_array;


- (instancetype) initWithArray: (const fleece::Array*)arrayValue
                      document: (UU FleeceDocument*)document
{
    NSParameterAssert(arrayValue);
    self = [super init];
    if (self) {
        _array = arrayValue;
        _count = _array->count();
        _document = document;
    }
    return self;
}


- (id) copyWithZone:(NSZone *)zone {
    return self;
}


- (NSMutableArray*) mutableCopy {
    NSMutableArray* m = [[NSMutableArray alloc] initWithCapacity: _count];
    for (Array::iterator iter(_array); iter; ++iter) {
        [m addObject: [_document objectForValue: iter.value()]];
    }
    return m;
}


- (NSUInteger) count {
    return _count;
}


- (id) objectAtIndex: (NSUInteger)index {
    if (index >= _count)
        [NSException raise: NSRangeException format: @"Array index out of range"];
    auto v = _array->get((uint32_t)index);
    return [_document objectForValue: v];
}


// Fast enumeration -- for(in) loops use this.
- (NSUInteger) countByEnumeratingWithState: (NSFastEnumerationState *)state
                                   objects: (id __unsafe_unretained [])stackBuf
                                     count: (NSUInteger)stackBufCount
{
    NSUInteger index = state->state;
    if (index == 0)
        state->mutationsPtr = &state->extra[0]; // this has to be pointed to something non-nullptr
    if (index >= _count)
        return 0;

    auto iter = _array->begin();
    iter += (uint32_t)index;
    NSUInteger n = 0;
    for (; iter; ++iter) {
        id v = [_document objectForValue: iter.value()];
        assert(v);
        CFAutorelease(CFBridgingRetain(v));
        stackBuf[n++] = v;
        if (n >= stackBufCount)
            break;
    }

    state->itemsPtr = stackBuf;
    state->state += n;
    return n;
}


// This is what the %@ substitution calls.
- (NSString *)descriptionWithLocale:(id)locale indent:(NSUInteger)level {
    NSMutableString* desc = [@"[\n" mutableCopy];
    for (auto iter = _array->begin(); iter; ++iter) {
        NSString* valStr = (NSString*)iter->toJSON();
        [desc appendFormat: @"    %@,\n", valStr];
    };
    [desc appendString: @"]"];
    return desc;
}


@end
