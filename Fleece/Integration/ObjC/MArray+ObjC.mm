//
// MArray+ObjC.mm
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#import "MArray+ObjC.h"
#import "MValue+ObjC.hh"
#import "MArray.hh"
#import "PlatformCompat.hh"
#include "betterassert.hh"

using namespace fleece;


@implementation FleeceArray
{
    MArray<id> _array;
}


- (instancetype) initWithMValue: (fleece::MValue<id>*)mv
                       inParent: (fleece::MCollection<id>*)parent
{
    self = [super init];
    if (self) {
        _array.initInSlot(mv, parent);
    }
    return self;
}


- (instancetype) initWithCopyOfMArray: (const MArray<id>&)mArray isMutable: (bool)isMutable {
    self = [super init];
    if (self) {
        _array.initAsCopyOf(mArray, isMutable);
    }
    return self;
}


- (id) copyWithZone:(NSZone *)zone {
    if (!_array.isMutable())
        return self;
    return [[[self class] alloc] initWithCopyOfMArray: _array isMutable: false];
}


- (NSMutableArray*) mutableCopyWithZone:(NSZone *)zone {
    return [[[self class] alloc] initWithCopyOfMArray: _array isMutable: true];
}


- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    SharedEncoder encoder(enc);
    _array.encodeTo(encoder);
}


- (MCollection<id>*) fl_collection {
    return &_array;
}


[[noreturn]] static void throwRangeException(NSUInteger index) {
    [NSException raise: NSRangeException format: @"Array index %lu is out of range",
                    (unsigned long)index];
    abort();
}


[[noreturn]] static void throwNilValueException() {
    [NSException raise: NSInvalidArgumentException format: @"Attempt to store nil value in array"];
    abort();
}


- (NSUInteger) count {
    return _array.count();
}


- (id) objectAtIndex: (NSUInteger)index {
    auto &val = _array.get(index);
    if (_usuallyFalse(val.isEmpty()))
        throwRangeException(index);
    return val.asNative(&_array);
}


#pragma mark - MUTATION:


- (void) requireMutable {
    if (!_array.isMutable())
        [NSException raise: NSInternalInconsistencyException format: @"Array is immutable"];
}


- (bool) isMutated {
    return _array.isMutated();
}


- (void)addObject:(UU id)anObject {
    if (_usuallyFalse(!_array.insert(_array.count(), anObject))) {
        [self requireMutable];
        throwNilValueException();
    }
}


- (void)insertObject:(UU id)anObject atIndex:(NSUInteger)index {
    if (_usuallyFalse(!_array.insert((uint32_t)index, anObject))) {
        [self requireMutable];
        if (!anObject)
            throwNilValueException();
        else
            throwRangeException(index);
    }
}


- (void)removeLastObject {
    if (_usuallyFalse(!_array.remove(_array.count()-1))) {
        [self requireMutable];
        throwRangeException(0);
    }
}


- (void)removeObjectAtIndex:(NSUInteger)index {
    [self removeObjectsInRange: {index, 1}];
}


- (void)removeObjectsInRange:(NSRange)range {
    if (_usuallyFalse(!_array.remove(range.location, range.length))) {
        [self requireMutable];
        throwRangeException(range.location + range.length - 1);
    }
}


- (void)replaceObjectAtIndex:(NSUInteger)index withObject:(UU id)anObject {
    if (_usuallyFalse(!_array.set((uint32_t)index, anObject))) {
        [self requireMutable];
        if (!anObject)
            throwNilValueException();
        else
            throwRangeException(index);
    }
}


- (void)removeAllObjects {
    if (_usuallyFalse(!_array.clear()))
        [self requireMutable];
}


// Fast enumeration -- for(in) loops use this.
- (NSUInteger) countByEnumeratingWithState: (NSFastEnumerationState *)state
                                   objects: (id __unsafe_unretained [])stackBuf
                                     count: (NSUInteger)stackBufCount
{
    NSUInteger index = state->state;
    if (index == 0)
        state->mutationsPtr = &state->extra[0]; // this has to be pointed to something non-nullptr
    NSUInteger count = _array.count();
    if (index >= count)
        return 0;

    NSUInteger n = 0;
    for (; index < count; ++index) {
        id v = _array.get(index).asNative(&_array);
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


#if 0
// This is what the %@ substitution calls.
- (NSString *)descriptionWithLocale:(id)locale indent:(NSUInteger)level {
    NSMutableString* desc = [@"[\n" mutableCopy];
    uint32_t count = _array.count();
    for (uint32_t i = 0; i < count; ++i) {
        [desc appendString: @"    "];
        auto mVal = _array.get(i);
        if (mVal.value())
            [desc appendString: (NSString*)mVal.value()->toJSON()];
        else
            [desc appendString: [mVal.asNative(&_array, _mutable) description]];
        [desc appendString: @",\n"];
    };
    [desc appendString: @"]"];
    return desc;
}
#endif


@end

