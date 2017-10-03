//
//  MutableArray.cc
//  Fleece
//
//  Created by Jens Alfke on 9/19/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "MutableArray.hh"
#include "varint.hh"

namespace fleece {

    using namespace internal;


    MutableArray::MutableArray(const Array *a)
    :MutableArray(a->count())
    {
        _source = a;
        if (a->isMutableArray()) {
            _items = ((MutableArray*)a)->_items;
        } else {
            auto dst = _items.begin();
            for (Array::iterator src(a); src; ++src, ++dst) {
                dst->set(src.value());
                dst->setChanged(false);
            }
        }
    }


    bool MutableArray::isChanged() const {
        if (_changed)
            return true;
        for (auto &item : _items)
            if (item.isChanged())
                return true;
        return false;
    }


    void MutableArray::resize(uint32_t newSize) {
        _items.resize(newSize);
        _changed = true;
    }


    void MutableArray::insert(uint32_t where, uint32_t n) {
        throwIf(where > count(), OutOfRange, "insert position is past end of array");
        _items.insert(_items.begin() + where,  n, MutableValue());
        _changed = true;
    }


    void MutableArray::remove(uint32_t where, uint32_t n) {
        throwIf(where + n > count(), OutOfRange, "remove range is past end of array");
        auto at = _items.begin() + where;
        _items.erase(at, at + n);
        _changed = true;
    }


    void MutableArray::removeAll() {
        _items.clear();
        _changed = true;
    }

}
