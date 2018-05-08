//
// HeapArray.cc
//
// Copyright © 2018 Couchbase. All rights reserved.
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

#include "HeapArray.hh"
#include "HeapDict.hh"
#include "MutableArray.hh"
#include "varint.hh"

namespace fleece { namespace internal {

    using namespace internal;


    HeapArray::HeapArray(const Array *a)
    :HeapCollection(kArrayTag)
    ,_items(a->count())
    ,_source(a)
    { }


    void HeapArray::populate(unsigned fromIndex) {
        if (!_source)
            return;
        auto dst = _items.begin() + fromIndex;
        Array::iterator src(_source);
        for (src += fromIndex; src && dst != _items.end(); ++src, ++dst) {
            if (!*dst)
                dst->set(src.value());
        }

    }


    const Value* HeapArray::get(uint32_t index) {
        if (index >= count())
            return nullptr;
        auto &item = _items[index];
        if (item)
            return item.asValue();
        assert(_source);
        return _source->get(index);
    }


    void HeapArray::resize(uint32_t newSize) {
        if (newSize == count())
            return;
        _items.resize(newSize, ValueSlot(Null()));
        setChanged(true);
    }


    void HeapArray::insert(uint32_t where, uint32_t n) {
        throwIf(where > count(), OutOfRange, "insert position is past end of array");
        if (n == 0)
            return;
        populate(where);
        _items.insert(_items.begin() + where,  n, ValueSlot(Null()));
        setChanged(true);
    }


    void HeapArray::remove(uint32_t where, uint32_t n) {
        throwIf(where + n > count(), OutOfRange, "remove range is past end of array");
        if (n == 0)
            return;
        populate(where + n);
        auto at = _items.begin() + where;
        _items.erase(at, at + n);
        setChanged(true);
    }


    HeapCollection* HeapArray::getMutable(uint32_t index, tags ifType) {
        if (index >= count())
            return nullptr;
        Retained<HeapCollection> result = nullptr;
        auto &mval = _items[index];
        if (mval) {
            result = mval.makeMutable(ifType);
        } else if (_source) {
            result = HeapCollection::mutableCopy(_source->get(index), ifType);
            if (result)
                _items[index].set(result->asValue());
        }
        if (result)
            setChanged(true);
        return result;
    }

    MutableArray* HeapArray::getMutableArray(uint32_t i)  {
        return (MutableArray*)asValue(getMutable(i, internal::kArrayTag));
    }

    /** Promotes a Dict item to a HeapDict (in place) and returns it.
     Or if the item is already a HeapDict, just returns it. Else returns null. */
    MutableDict* HeapArray::getMutableDict(uint32_t i) {
        return (MutableDict*)asValue(getMutable(i,internal::kDictTag));
    }



    internal::ValueSlot& HeapArray::_appendMutableValue() {
        setChanged(true);
        _items.emplace_back();
        return _items.back();
    }


    const ValueSlot* HeapArray::first() {
        populate(0);
        return &_items.front();
    }



    HeapArray::iterator::iterator(const HeapArray *ma) noexcept
    :_iter(ma->_items.begin())
    ,_iterEnd(ma->_items.end())
    ,_sourceIter(ma->_source)
    {
        ++(*this);
    }

    HeapArray::iterator::iterator(const MutableArray *ma) noexcept
    :iterator((HeapArray*)HeapCollection::asHeapValue(ma))
    { }

    HeapArray::iterator& HeapArray::iterator::operator ++() {
        if (_iter == _iterEnd) {
            _value = nullptr;
        } else {
            _value = _iter->asValue();
            if (!_value)
                _value = _sourceIter[_index];
            ++_iter;
            ++_index;
        }
        return *this;
    }


} }
