//
//  MutableArray+ObjC.mm
//  Fleece
//
//  Created by Jens Alfke on 5/28/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#import "MutableArray+ObjC.h"
#import "MValue+ObjC.hh"
#import "MArray.hh"
#import "PlatformCompat.hh"

using namespace fleeceapi;


@implementation FleeceArray
{
    MArray<id> _array;
    bool _mutable;
}


- (instancetype) init {
    self = [super init];
    if (self) {
        _mutable = true;
    }
    return self;
}


- (instancetype) initWithMValue: (fleeceapi::MValue<id>*)mv
                       inParent: (fleeceapi::MCollection<id>*)parent
                      isMutable: (bool)isMutable
{
    self = [super init];
    if (self) {
        _array.initInSlot(mv, parent);
        _mutable = isMutable;
    }
    return self;
}


- (instancetype) initWithCopyOfMArray: (const MArray<id>&)mArray isMutable: (bool)isMutable {
    self = [super init];
    if (self) {
        _array.init(mArray);
        _mutable = isMutable;
    }
    return self;
}


- (id) copyWithZone:(NSZone *)zone {
    if (!_mutable)
        return self;
    return [[[self class] alloc] initWithCopyOfMArray: _array isMutable: false];
}


- (NSMutableArray*) mutableCopyWithZone:(NSZone *)zone {
    return [[[self class] alloc] initWithCopyOfMArray: _array isMutable: true];
}


- (void) fl_encodeToFLEncoder: (FLEncoder)enc {
    Encoder encoder(enc);
    _array.encodeTo(encoder);
    encoder.release();
}


- (MCollection<id>*) fl_collection {
    return &_array;
}


[[noreturn]] static void throwRangeException(NSUInteger index) {
    [NSException raise: NSRangeException format: @"Array index %zu is out of range", index];
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


- (bool) isMutated {
    return _array.isMutated();
}


- (void)addObject:(UU id)anObject {
    NSParameterAssert(_mutable);
    if (_usuallyFalse(!_array.insert(_array.count(), anObject)))
        throwNilValueException();
}


- (void)insertObject:(UU id)anObject atIndex:(NSUInteger)index {
    NSParameterAssert(_mutable);
    if (_usuallyFalse(!_array.insert((uint32_t)index, anObject))) {
        if (!anObject)
            throwNilValueException();
        else
            throwRangeException(index);
    }
}


- (void)removeLastObject {
    NSParameterAssert(_mutable);
    if (_usuallyFalse(!_array.remove(_array.count()-1)))
        throwRangeException(0);
}


- (void)removeObjectAtIndex:(NSUInteger)index {
    [self removeObjectsInRange: {index, 1}];
}


- (void)removeObjectsInRange:(NSRange)range {
    NSParameterAssert(_mutable);
    if (_usuallyFalse(!_array.remove(range.location, range.length)))
        throwRangeException(range.location + range.length - 1);
}


- (void)replaceObjectAtIndex:(NSUInteger)index withObject:(UU id)anObject {
    NSParameterAssert(_mutable);
    if (_usuallyFalse(!_array.set((uint32_t)index, anObject))) {
        if (!anObject)
            throwNilValueException();
        else
            throwRangeException(index);
    }
}


- (void)removeAllObjects {
    NSParameterAssert(_mutable);
    _array.clear();
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
        id v = _array.get(index).asNative(_array.parent());
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

