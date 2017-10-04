//
//  MutableDict+ObjC.mm
//  Fleece
//
//  Created by Jens Alfke on 5/28/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "MutableDict+ObjC.hh"

using namespace fleece;

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
        _dict.init(mv, parent);
        _mutable = isMutable;
    }
    return self;
}

- (instancetype) initWithMDict: (const MDict<id>&)mDict isMutable: (bool)isMutable {
    self = [super init];
    if (self) {
        _dict = mDict;
        _mutable = isMutable;
        NSLog(@"INIT FleeceDict %p", self);
    }
    return self;
}


- (instancetype) copyWithZone:(NSZone *)zone {
    if (_mutable)
        return [[[self class] alloc] initWithMDict: _dict isMutable: false];
    else
        return self;
}


- (instancetype) mutableCopyWithZone:(NSZone *)zone {
    return [[[self class] alloc] initWithMDict: _dict isMutable: true];
}


- (void) setSlot: (fleece::MValue<id>*)newSlot from: (fleece::MValue<id>*)oldSlot {
    _dict.setSlot(newSlot, oldSlot);
}


- (void) fl_encodeTo: (Encoder*)enc {
    _dict.encodeTo(*enc);
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
    id result = value ? value->asNative(&_dict, _mutable) : nil;
    NSLog(@"objectForKey: '%@' = %@", key, result);//TEMP
    return result;
}


- (void)setObject:(id)value forKey:(id)key {
    NSParameterAssert(_mutable);
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
    NSLog(@"allKeys = %@", keys);//TEMP
    return keys;
}


- (NSEnumerator *)keyEnumerator {
    return self.allKeys.objectEnumerator;
}


- (void) enumerateKeysAndObjectsUsingBlock: (void (^)(UU id key, UU id obj, BOOL *stop))block {
    __block BOOL stop = NO;
    _dict.enumerate(^(slice key, const MValue<id> &value) {
        if (!stop)
            block((NSString*)key, value.asNative(&_dict, _mutable), &stop);
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
#endif


// This is what the %@ substitution calls.
- (NSString *)descriptionWithLocale:(id)locale indent:(NSUInteger)level {
    NSMutableString* desc = [@"{\n" mutableCopy];
    _dict.enumerate(^(slice key, const MValue<id> &value) {
        for (NSUInteger i = 0; i <= level; i++)
            [desc appendString: @"    "];
        [desc appendFormat: @"\"%.*s\": %@,\n",
                        (int)key.size, key.buf,
                        [value.asNative(&_dict, _mutable) descriptionWithLocale: locale /*indent: level+1*/]];
    });
    for (NSUInteger i = 0; i < level; i++)
        [desc appendString: @"    "];
    [desc appendString: @"}"];
    return desc;
}

@end
