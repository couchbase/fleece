//
//  stringTable.cc
//  Fleece
//
//  Created by Jens Alfke on 12/2/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//

#include "stringTable.hh"
#include <algorithm>
#include <assert.h>
#include <stdlib.h>

namespace fleece {

    static const float kMaxLoad = 0.666f;

    StringTable::StringTable(size_t capacity) {
        _count = 0;
        size_t size;
        for (size = 8; size*kMaxLoad < capacity; size *= 2)
            ;
        allocTable(size); // initializes _table, _size, _maxCount
    }

    StringTable::~StringTable() {
        ::free(_table);
    }

    void StringTable::clear() {
        ::memset(_table, 0, _size * sizeof(slot));
        _count = 0;
    }

    StringTable::slot* StringTable::find(fleece::slice key, uint32_t hash) {
        assert(key.buf != NULL);
        size_t index = hash & (_size - 1);
        slot *s = &_table[index];
        if (__builtin_expect(s->first.buf != NULL && s->first != key, false)) {
            slot *end = &_table[_size];
            do {
                if (++s >= end)
                    s = &_table[0];
            } while (s->first.buf != NULL && s->first != key);
        }
        if (s->first.buf == NULL) {
            s->second.hash = hash;
        }
        return s;
    }

    bool StringTable::_add(fleece::slice key, uint32_t h, const info& n) {
        auto s = find(key, h);
        if (s->first.buf)
            return false;
        else {
            s->first = key;
            s->second = n;
            s->second.hash = h;
            return true;
        }
    }

    void StringTable::addAt(slot* s, slice key, const info& n) {
        assert(key.buf != NULL);
        auto ss = const_cast<slot*>(s);
        assert(ss->first.buf == NULL);
        ss->first = key;
        auto hash = ss->second.hash;
        ss->second = n;
        ss->second.hash = hash;
        incCount();
    }

    void StringTable::add(fleece::slice key, const info& n) {
        if (_add(key, hash(key), n))
            incCount();
    }

    void StringTable::allocTable(size_t size) {
        auto table = (slot*)::calloc(size, sizeof(slot));
        if (!table)
            throw std::bad_alloc();
        _table = table;
        _size = size;
        _maxCount = (size_t)(size * kMaxLoad);
    }

    void StringTable::grow() {
        slot *oldTable = _table, *end = &_table[_size];
        allocTable(2*_size);
        for (auto s = oldTable; s < end; ++s) {
            if (s->first.buf != NULL)
                _add(s->first, s->second.hash, s->second);
        }
        ::free(oldTable);
    }

}
