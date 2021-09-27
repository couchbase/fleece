//
// MDict+ObjC.mm
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "MDict+ObjC.h"
#import "MValue+ObjC.hh"
#import "MDict.hh"
#import "MDictIterator.hh"
#include "PlatformCompat.hh"

using namespace fleece;


@implementation FleeceDict
{
    MDict<id> _dict;
}


- (instancetype) initWithMValue: (MValue<id>*)mv
                       inParent: (MCollection<id>*)parent
{
    self = [super init];
    if (self) {
        _dict.initInSlot(mv, parent);
    }
    return self;
}


- (instancetype) initWithCopyOfMDict: (const MDict<id>&)mDict
                           isMutable: (bool)isMutable
{
    self = [super init];
    if (self) {
        _dict.initAsCopyOf(mDict, isMutable);
    }
    return self;
}


- (instancetype) copyWithZone:(NSZone *)zone {
    if (!_dict.isMutable())
        return self;
    return [[[self class] alloc] initWithCopyOfMDict: _dict isMutable: false];
}


- (instancetype) mutableCopyWithZone:(NSZone *)zone {
    return [[[self class] alloc] initWithCopyOfMDict: _dict isMutable: true];
}


- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    SharedEncoder encoder(enc);
    _dict.encodeTo(encoder);
}


- (MCollection<id>*) fl_collection {
    return &_dict;
}


- (NSUInteger) count {
    return _dict.count();
}


- (BOOL)containsObjectForKey:(UU NSString *)key {
    return [key isKindOfClass: [NSString class]] && _dict.contains(nsstring_slice(key));
}


- (id) objectForKey: (UU id)key {
    if (![key isKindOfClass: [NSString class]])
        return nil;
    return _dict.get(nsstring_slice(key)).asNative(&_dict);
}


#pragma mark - MUTATION:


[[noreturn]] static void throwMutationException() {
    [NSException raise: NSInternalInconsistencyException format: @"Dictionary is immutable"];
    abort();
}


- (bool) isMutated {
    return _dict.isMutated();
}


- (void) checkNoParent: (UU id)value {
    auto collection = [value fl_collection];
    if (collection) {
        if (collection->parent()) {
            [NSException raise: NSInternalInconsistencyException
                        format: @"Can't add %@ %p to %@ %p, it's already in a collection",
                                 [value class], value, [self class], self];
        }
    }
}


- (void)setObject:(id)value forKey:(id<NSCopying>)key {
    //[self checkNoParent: value];
    if (!_dict.set(nsstring_slice((NSString*)key), value))
        throwMutationException();
}


- (void)removeObjectForKey:(id)key {
    if (!_dict.remove(nsstring_slice(key)))
        throwMutationException();
}


- (void)removeAllObjects {
    if (!_dict.clear())
        throwMutationException();
}


- (NSArray*) allKeys {
    NSMutableArray* keys = [NSMutableArray arrayWithCapacity: _dict.count()];
    for (MDict<id>::iterator i(_dict); i; ++i)
        [keys addObject: i.key().asNSString()];
    return keys;
}


- (NSEnumerator *)keyEnumerator {
    return self.allKeys.objectEnumerator;
}


- (void) enumerateKeysAndObjectsUsingBlock: (void (NS_NOESCAPE ^)(UU id key, UU id obj, BOOL *stop))block {
    __block BOOL stop = NO;
    for (MDict<id>::iterator i(_dict); i; ++i) {
        block(i.key().asNSString(), i.nativeValue(), &stop);
        if (stop)
            break;
    }
}


#if 0
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

    auto iter = _dict.begin();
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
#endif


#if 0
// This is what the %@ substitution calls.
- (NSString *)descriptionWithLocale:(id)locale indent:(NSUInteger)level {
    NSMutableString* desc = [@"{\n" mutableCopy];
    __block NSUInteger n = 0;
    _dict.enumerate(^(slice key, const MValue<id> &mValue) {
        if (n++ > 0)
            [desc appendString: @",\n"];
        for (NSUInteger i = 0; i <= level; i++)
            [desc appendString: @"    "];
        id value = mValue.asNative(_dict, _mutable);
        NSString* valueDesc;
        if ([value respondsToSelector: @selector(descriptionWithLocale:indent:)])
            valueDesc = [value descriptionWithLocale: locale indent: level+1];
        else if ([value respondsToSelector: @selector(descriptionWithLocale:)])
            valueDesc = [value descriptionWithLocale: locale];
        else
            valueDesc = [value description];
        [desc appendFormat: @"\"%.*s\": %@", (int)key.size, key.buf, valueDesc];
    });
    [desc appendString: @"\n"];
    for (NSUInteger i = 0; i < level; i++)
        [desc appendString: @"    "];
    [desc appendString: @"}"];
    return desc;
}
#endif

@end
