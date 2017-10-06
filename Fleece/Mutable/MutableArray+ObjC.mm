//
//  MutableArray+ObjC.cc
//  Fleece
//
//  Created by Jens Alfke on 5/28/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "MutableArray+ObjC.hh"
#include "MutableDict+ObjC.hh"

#define UU __unsafe_unretained


// Defines an fl_collection property that returns null by default.
// FleeceArray and FleeceDictionary override this to return their MCollection instance.
// This is used by the implementation of MValue<id>::encodeNative(), below.
@interface NSObject (Fleece)
@property (readonly, nonatomic) fleece::MCollection<id>* fl_collection;
@end

@implementation NSObject (Fleece)
- (fleece::MCollection<id>*) fl_collection {
    return nullptr;
}
@end


namespace fleece {

    // These are the three MValue methods that have to be implemented in any specialization,
    // here specialized for <id>.

    template<>
    id MValue<id>::toNative(MCollection<id> *parent) {
        switch (_value->type()) {
            case kArray:
                return _native = [[FleeceArray alloc] initWithMValue: this
                                                            inParent: parent
                                                           isMutable: parent->mutableContainers()];
            case kDict:
                return _native = [[FleeceDict alloc] initWithMValue: this
                                                           inParent: parent
                                                          isMutable: parent->mutableContainers()];
            default:
                return /*_native =*/ _value->toNSObject();
        }
    }

    template<>
    MCollection<id>* MValue<id>::collectionFromNative(id native) {
        return [native fl_collection];
    }

    template<>
    void MValue<id>::encodeNative(Encoder &enc, id obj) {
        enc.writeObjC(obj);
    }

}


using namespace fleece;

@implementation FleeceArray
{
    MArray<id> _array;
    bool _mutable;
}

- (instancetype) initWithMValue: (fleece::MValue<id>*)mv
                       inParent: (fleece::MCollection<id>*)parent
                      isMutable: (bool)isMutable
{
    self = [super init];
    if (self) {
        _array.init(mv, parent);
        _mutable = isMutable;
        NSLog(@"INIT FleeceArray %p with parent=%p", self, mv);
    }
    return self;
}

- (instancetype) initWithCopyOfMArray: (const MArray<id>&)mArray isMutable: (bool)isMutable {
    self = [super init];
    if (self) {
        _array.init(mArray);
        _mutable = isMutable;
        NSLog(@"INIT FleeceArray %p", self);
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


- (void) fl_encodeTo: (Encoder*)enc {
    _array.encodeTo(*enc);
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
    auto &val = _array.get((uint32_t)index);
    if (_usuallyFalse(val.isEmpty()))
        throwRangeException(index);
    return val.asNative(&_array);
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
    if (!_array.remove(_array.count()-1))
        throwRangeException(0);
}


- (void)removeObjectAtIndex:(NSUInteger)index {
    NSParameterAssert(_mutable);
    if (_usuallyFalse(!_array.remove((uint32_t)index)))
        throwRangeException(index);
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
#endif


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

