//
//  MutableDict+ObjC.mm
//  Fleece
//
//  Created by Jens Alfke on 5/28/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "MutableDict+ObjC.hh"
#include "MutableArray+ObjC.hh"
#include "PlatformCompat.hh"

using namespace fleece;
using namespace fleeceapi;

#define UU __unsafe_unretained


@implementation FleeceDict
{
    MDict<id> _dict;
    bool _mutable;
}

- (instancetype) initWithMValue: (MValue<id>*)mv
                       inParent: (MCollection<id>*)parent
                      isMutable: (bool)isMutable {
    self = [super init];
    if (self) {
        _dict.initInSlot(mv, parent);
        _mutable = isMutable;
    }
    return self;
}

- (instancetype) initWithCopyOfMDict: (const MDict<id>&)mDict isMutable: (bool)isMutable {
    self = [super init];
    if (self) {
        _dict = mDict;              // this copies mDict into _dict
        _mutable = isMutable;
        NSLog(@"INIT FleeceDict %p", self);
    }
    return self;
}


- (instancetype) copyWithZone:(NSZone *)zone {
    if (!_mutable)
        return self;
    return [[[self class] alloc] initWithCopyOfMDict: _dict isMutable: false];
}


- (instancetype) mutableCopyWithZone:(NSZone *)zone {
    return [[[self class] alloc] initWithCopyOfMDict: _dict isMutable: true];
}


- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    Encoder encoder(enc);
    _dict.encodeTo(encoder);
    encoder.release();
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
    auto value = _dict.get(nsstring_slice(key));
    return value ? value->asNative(&_dict) : nil;
}


#pragma mark - MUTATION:


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


- (void)setObject:(id)value forKey:(id)key {
    NSParameterAssert(_mutable);
    //[self checkNoParent: value];
    _dict.set(nsstring_slice(key), value);
}


- (void)removeObjectForKey:(id)key {
    NSParameterAssert(_mutable);
    _dict.remove(nsstring_slice(key));
}


- (void)removeAllObjects {
    NSParameterAssert(_mutable);
    _dict.clear();
}


- (NSArray*) allKeys {
    NSMutableArray* keys = [NSMutableArray arrayWithCapacity: _dict.count()];
    _dict.enumerate(^(slice key, const MValue<id> &value) {
        [keys addObject: (NSString*)key];
    });
    return keys;
}


- (NSEnumerator *)keyEnumerator {
    return self.allKeys.objectEnumerator;
}


- (void) enumerateKeysAndObjectsUsingBlock: (void (^)(UU id key, UU id obj, BOOL *stop))block {
    __block BOOL stop = NO;
    _dict.enumerate(^(slice key, const MValue<id> &value) {
        if (!stop)
            block((NSString*)key, value.asNative(&_dict), &stop);
    });
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
