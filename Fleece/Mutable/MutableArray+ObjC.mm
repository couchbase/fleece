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


namespace fleece {

    template<>
    id MValue<id>::toNative(MCollection<id> *parent, bool asMutable) {
        switch (_value->type()) {
            case kArray:
                return _native = [[FleeceArray alloc] initWithMValue: this
                                                            inParent: parent
                                                           isMutable: asMutable];
            case kDict:
                return _native = [[FleeceDict alloc] initWithMValue: this
                                                           inParent: parent
                                                          isMutable: asMutable];
            default:
                return /*_native =*/ _value->toNSObject();
        }
    }

    template<>
    void MValue<id>::nativeChangeSlot(MValue<id> *newSlot) {
        if (_value || [_native isKindOfClass: [FleeceArray class]]
                   || [_native isKindOfClass: [FleeceDict class]]) {
            [_native setSlot: newSlot from: this];
        }
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

- (instancetype) initWithMArray: (const MArray<id>&)mArray {
    self = [super init];
    if (self) {
        _array.init(mArray);
        _mutable = true;
        NSLog(@"INIT FleeceArray %p", self);
    }
    return self;
}


- (id) copyWithZone:(NSZone *)zone {
    if (_mutable)
        return [[[self class] alloc] initWithMArray: _array];
    else
        return self;
}


- (NSMutableArray*) mutableCopyWithZone:(NSZone *)zone {
    return [[[self class] alloc] initWithMArray: _array];
}


- (void) setSlot: (MValue<id>*)newSlot from: (MValue<id>*)oldSlot {
    NSLog(@"FleeceArray %p: change parent from=%p to=%p", self, oldSlot, newSlot);
    _array.setSlot(newSlot, oldSlot);
}


- (NSUInteger) count {
    return _array.count();
}


- (id) objectAtIndex: (NSUInteger)index {
    return _array.get((uint32_t)index).asNative(&_array, _mutable);
}


- (void) fl_encodeTo: (Encoder*)enc {
    _array.encodeTo(*enc);
}


- (void)addObject:(UU id)anObject {
    NSParameterAssert(_mutable);
    NSParameterAssert(anObject != nil);
    _array.insert(_array.count(), anObject);
}


- (void)insertObject:(UU id)anObject atIndex:(NSUInteger)index {
    NSParameterAssert(_mutable);
    NSParameterAssert(index <= _array.count());
    NSParameterAssert(anObject != nil);
    _array.insert((uint32_t)index, anObject);
}


- (void)removeLastObject {
    NSParameterAssert(_mutable);
    NSParameterAssert(_array.count() > 0);
    _array.remove(_array.count()-1);
}


- (void)removeObjectAtIndex:(NSUInteger)index {
    NSParameterAssert(_mutable);
    NSParameterAssert(index < _array.count());
    _array.remove((uint32_t)index);
}


- (void)replaceObjectAtIndex:(NSUInteger)index withObject:(id)anObject {
    NSParameterAssert(_mutable);
    NSParameterAssert(index < _array.count());
    NSParameterAssert(anObject != nil);
    _array.set((uint32_t)index, anObject);
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
#endif


@end

