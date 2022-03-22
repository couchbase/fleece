//
// Array.cc
//
// Copyright 2016-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "Array.hh"
#include "MutableArray.hh"
#include "HeapDict.hh"
#include "Internal.hh"
#include "fleece/PlatformCompat.hh"
#include "varint.hh"


namespace fleece { namespace impl {
    using namespace internal;


#pragma mark - ARRAY::IMPL:


    __hot
    Array::impl::impl(const Value* v) noexcept {
        if (_usuallyFalse(v == nullptr)) {
            _first = nullptr;
            _width = kNarrow;
            _count = 0;
        } else if (_usuallyTrue(!v->isMutable())) {
            // Normal immutable case:
            _first = (const Value*)(&v->_byte[2]);
            _width = v->isWideArray() ? kWide : kNarrow;
            _count = v->countValue();
            if (_usuallyFalse(_count == kLongArrayCount)) {
                // Long count is stored as a varint:
                uint32_t extraCount;
                size_t countSize = GetUVarInt32(slice(_first, 10), &extraCount);
                if (_usuallyTrue(countSize > 0))
                    _count += extraCount;
                else
                    _count = 0;     // invalid data, but I'm not allowed to throw an exception
                _first = offsetby(_first, countSize + (countSize & 1));
            }
        } else {
            // Mutable Array or Dict:
            auto mcoll = (HeapCollection*)HeapValue::asHeapValue(v);
            HeapArray *mutArray;
            if (v->tag() == kArrayTag) {
                mutArray = (HeapArray*)mcoll;
                _count = mutArray->count();
            } else {
                mutArray = ((HeapDict*)mcoll)->kvArray();
                _count = mutArray->count() / 2;
            }
            _first = _count ? (const Value*)mutArray->first() : nullptr;
            _width = sizeof(ValueSlot);
        }
    }

    __hot
    const Value* Array::impl::deref(const Value *v) const noexcept {
        if (_usuallyFalse(isMutableArray()))
            return ((ValueSlot*)v)->asValue();
        return v->deref(_width == kWide);
    }

    __hot
    const Value* Array::impl::operator[] (unsigned index) const noexcept {
        if (_usuallyFalse(index >= _count))
            return nullptr;
        if (_width == kNarrow)
            return offsetby(_first, kNarrow * index)->deref<false>();
        else if (_usuallyTrue(_width == kWide))
            return offsetby(_first, kWide   * index)->deref<true>();
        else
            return ((ValueSlot*)_first + index)->asValue();
    }

    const Value* Array::impl::firstValue() const noexcept {
        if (_usuallyFalse(_count == 0))
            return nullptr;
        return deref(_first);
    }

    size_t Array::impl::indexOf(const Value *v) const noexcept {
        return ((size_t)v - (size_t)_first) / _width;
    }

    __hot
    void Array::impl::offset(uint32_t n) {
        throwIf(n > _count, OutOfRange, "iterating past end of array");
        _count -= n;
        if (_usuallyTrue(_count > 0))
            _first = offsetby(_first, _width*n);
    }


#pragma mark - ARRAY:


    uint32_t Array::count() const noexcept {
        if (_usuallyFalse(isMutable()))
            return heapArray()->count();
        return impl(this)._count;
    }

    bool Array::empty() const noexcept {
        if (_usuallyFalse(isMutable()))
            return heapArray()->empty();
        return countIsZero();
    }

    const Value* Array::get(uint32_t index) const noexcept {
        if (_usuallyFalse(isMutable()))
            return heapArray()->get(index);
        return impl(this)[index];
    }

    HeapArray* Array::heapArray() const {
        return (HeapArray*)internal::HeapCollection::asHeapValue(this);
    }

    MutableArray* Array::asMutable() const {
        return isMutable() ? (MutableArray*)this : nullptr;
    }

    EVEN_ALIGNED static constexpr Array kEmptyArrayInstance;
    const Array* const Array::kEmpty = &kEmptyArrayInstance;


#pragma mark - ARRAY::ITERATOR:
    

    ArrayIterator::ArrayIterator(const Array *a) noexcept
    :impl(a),
     _value(firstValue())
    { }

    ArrayIterator& ArrayIterator::operator++() {
        offset(1);
        _value = firstValue();
        return *this;
    }

    ArrayIterator& ArrayIterator::operator += (uint32_t n) {
        offset(n);
        _value = firstValue();
        return *this;
    }

} }
