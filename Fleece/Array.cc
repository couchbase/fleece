//
//  Array.cc
//  Fleece
//
//  Created by Jens Alfke on 5/12/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "Array.hh"
#include "Internal.hh"
#include "PlatformCompat.hh"
#include "varint.hh"


namespace fleece {
    using namespace internal;


    Array::impl::impl(const Value* v) noexcept {
        if (v == nullptr) {
            _first = nullptr;
            _wide = false;
            _count = 0;
            return;
        }
        
        _first = (const Value*)(&v->_byte[2]);
        _wide = v->isWideArray();
        _count = v->shortValue() & 0x07FF;
        if (_count == kLongArrayCount) {
            // Long count is stored as a varint:
            uint32_t extraCount;
            size_t countSize = GetUVarInt32(slice(_first, 10), &extraCount);
            assert(countSize > 0);
            _count += extraCount;
            _first = offsetby(_first, countSize + (countSize & 1));
        }
    }

    bool Array::impl::next() {
        throwIf(_count == 0, OutOfRange, "iterating past end of array");
        if (--_count == 0)
            return false;
        _first = _first->next(_wide);
        return true;
    }

    const Value* Array::impl::operator[] (unsigned index) const noexcept {
        if (index >= _count)
            return nullptr;
        if (_wide)
            return Value::deref<true> (offsetby(_first, kWide   * index));
        else
            return Value::deref<false>(offsetby(_first, kNarrow * index));
    }

    size_t Array::impl::indexOf(const Value *v) const noexcept {
        return ((size_t)v - (size_t)_first) / width(_wide);
    }



    uint32_t Array::count() const noexcept {
        return Array::impl(this)._count;
    }

    const Value* Array::get(uint32_t index) const noexcept {
        return impl(this)[index];
    }

    static constexpr Array kEmptyArrayInstance;
    const Array* const Array::kEmpty = &kEmptyArrayInstance;



    Array::iterator::iterator(const Array *a) noexcept
    :_a(a),
     _value(_a.firstValue())
    { }

    Array::iterator& Array::iterator::operator++() {
        _a.next();
        _value = _a.firstValue();
        return *this;
    }

    Array::iterator& Array::iterator::operator += (uint32_t n) {
        throwIf(n > _a._count, OutOfRange, "iterating past end of array");
        _a._count -= n;
        _a._first = offsetby(_a._first, width(_a._wide)*n);
        _value = _a.firstValue();
        return *this;
    }

}
