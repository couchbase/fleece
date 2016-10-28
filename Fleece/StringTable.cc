//
//  StringTable.cc
//  Fleece
//
//  Created by Jens Alfke on 12/2/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "StringTable.hh"
#include "PlatformCompat.hh"
#include <algorithm>
#include <assert.h>
#include <stdlib.h>

namespace fleece {

    static_assert(sizeof(StringTable::info) == 8, "info isn't packed");

    static const float kMaxLoad = 0.666f;

    StringTable::StringTable(size_t capacity) {
        _count = 0;
        size_t size;
        for (size = 16; size*kMaxLoad < capacity; size *= 2)
            ;
        allocTable(size); // initializes _table, _size, _maxCount
    }

    StringTable::~StringTable() {
        if (_table != _initialTable)
            ::free(_table);
    }

    void StringTable::clear() noexcept {
        ::memset(_table, 0, _size * sizeof(slot));
        _count = 0;
    }

    StringTable::slot& StringTable::find(fleece::slice key, uint32_t hash) const noexcept {
        assert(key.buf != nullptr);
        size_t index = hash & (_size - 1);
        slot *s = &_table[index];
        if (_usuallyFalse(s->first.buf != nullptr && s->first != key)) {
            slot *end = &_table[_size];
            do {
                if (++s >= end)
                    s = &_table[0];
            } while (s->first.buf != nullptr && s->first != key);
        }
        if (s->first.buf == nullptr) {
            s->second.hash = hash;
        }
        return *s;
    }

    bool StringTable::_add(fleece::slice key, uint32_t h, const info& n) noexcept {
        slot &s = find(key, h);
        if (s.first.buf)
            return false;
        else {
            s.first = key;
            s.second = n;
            s.second.hash = h;
            return true;
        }
    }

    void StringTable::addAt(slot& s, slice key, const info& n) noexcept {
        assert(key.buf != nullptr);
        assert(s.first.buf == nullptr);
        s.first = key;
        auto hash = s.second.hash;
        s.second = n;
        s.second.hash = hash;
        incCount();
    }

    void StringTable::add(fleece::slice key, const info& n) {
        if (_add(key, key.hash(), n))
            incCount();
    }

    void StringTable::allocTable(size_t size) {
        slot* table;
        if (size <= kInitialTableSize) {
            table = _initialTable;
            memset(table, 0, sizeof(_initialTable));
            size = kInitialTableSize;
        } else {
            table = (slot*)::calloc(size, sizeof(slot));
            if (!table)
                throw std::bad_alloc();
        }
        _table = table;
        _size = size;
        _maxCount = (size_t)(size * kMaxLoad);
    }

    void StringTable::grow() {
        slot *oldTable = _table, *end = &_table[_size];
        allocTable(2*_size);
        for (auto s = oldTable; s < end; ++s) {
            if (s->first.buf != nullptr)
                _add(s->first, s->second.hash, s->second);
        }
        if (oldTable != _initialTable)
            ::free(oldTable);
    }

}
