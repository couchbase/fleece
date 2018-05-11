//
// Array.cc
//
// Copyright (c) 2016 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "Array.hh"
#include "Internal.hh"
#include "PlatformCompat.hh"
#include "varint.hh"
#include <vector>


namespace fleece {
    using namespace internal;


    Array::impl::impl(const Value* v) noexcept {
        if (_usuallyFalse(v == nullptr)) {
            _first = nullptr;
            _wide = false;
            _count = 0;
            return;
        }
        
        _first = (const Value*)(&v->_byte[2]);
        _wide = v->isWideArray();
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
    }

    const Value* Array::impl::operator[] (unsigned index) const noexcept {
        if (_usuallyFalse(index >= _count))
            return nullptr;
        if (_wide)
            return Value::deref<true> (offsetby(_first, kWide   * index));
        else
            return Value::deref<false>(offsetby(_first, kNarrow * index));
    }

    const Value* Array::impl::firstValue() const noexcept {
        if (_usuallyFalse(_count == 0))
            return nullptr;
        return Value::deref(_first, _wide);
    }

    size_t Array::impl::indexOf(const Value *v) const noexcept {
        return ((size_t)v - (size_t)_first) / width(_wide);
    }



    uint32_t Array::count() const noexcept {
        return impl(this)._count;
    }

    const Value* Array::get(uint32_t index) const noexcept {
        return impl(this)[index];
    }

    static constexpr Array kEmptyArrayInstance;
    const Array* const Array::kEmpty = &kEmptyArrayInstance;



    Array::iterator::iterator(const Array *a) noexcept
    :impl(a),
     _value(firstValue())
    { }

    Array::iterator& Array::iterator::operator++() {
        throwIf(_count == 0, OutOfRange, "iterating past end of array");
        if (_usuallyTrue(--_count > 0))
            _first = _first->next(_wide);
        _value = firstValue();
        return *this;
    }

    Array::iterator& Array::iterator::operator += (uint32_t n) {
        throwIf(n > _count, OutOfRange, "iterating past end of array");
        _count -= n;
        if (_usuallyTrue(_count > 0))
            _first = offsetby(_first, width(_wide)*n);
        _value = firstValue();
        return *this;
    }

}
