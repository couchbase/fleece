//
// StringTable.cc
//
// Copyright (c) 2015 Couchbase, Inc All rights reserved.
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

#include "StringTable.hh"
#include "PlatformCompat.hh"
#include <algorithm>
#include <stdlib.h>
#include "betterassert.hh"

namespace fleece {

    static const float kMaxLoad = 0.9f;

    StringTable::StringTable(size_t capacity) {
        _count = 0;
        size_t size;
        for (size = kInitialTableSize; size*kMaxLoad < capacity; size *= 2)
            ;
        allocTable(size); // initializes _table, _size, _maxCount
    }

    StringTable::~StringTable() {
        if (_table.hashes != _initialHashes)
            _table.free();
    }

    void StringTable::clear() noexcept {
        ::memset(_table.hashes, 0, _size * sizeof(hash_t));
        _count = 0;
        _maxDistance = 0;
    }

    StringTable::value_t* StringTable::get(fleece::slice key, hash_t hash) const noexcept {
        position_t i = find(key, hash);
        return (i != kNoPosition) ? &_table.values[i] : nullptr;
    }

    StringTable::position_t StringTable::find(fleece::slice key, hash_t hash) const noexcept {
        assert(key.buf != nullptr);
        assert(hash != 0);
        size_t end = (hash + _maxDistance + 1) & _sizeMask;
        for (size_t i = (hash & _sizeMask); i != end; i = (i + 1) & _sizeMask) {
            if (_table.hashes[i] == 0)
                break;
            else if (_table.hashes[i] == hash && _table.keys[i] == key)
                return i;
        }
        //printf("Couldn't find '%.*s'\n", FMTSLICE(key));
        return kNoPosition;
    }

    void StringTable::add(slice key, hash_t hash, value_t value) {
        if (++_count > _maxCount)
            grow();
        _add(key, hash, value);
    }

    // subroutine of add() that doesn't bump the count or grow the table. Used in grow().
    void StringTable::_add(slice key, hash_t hash, value_t value) noexcept {
        assert(key.buf != nullptr);
        assert(hash != 0);
        ssize_t distance = 0;
        auto maxDistance = _maxDistance;
        size_t i = hash & _sizeMask;
        //printf("Adding '%.*s' starting at %zu ...\n", FMTSLICE(key), i);
        for (; _table.hashes[i] != 0; i = (i + 1) & _sizeMask) {
            assert(_table.keys[i] != key);
            assert(distance <= _count);
            ssize_t itsDistance = i - (_table.hashes[i] & _sizeMask);
            itsDistance = (itsDistance + _size) & _sizeMask;        // handle wraparound
            if (itsDistance < distance) {
                // Robin Hood hashing: Swap new item with less-distant existing item:
                //printf("    put '%.*s' at %zu (dist=%zd)\n", FMTSLICE(key), i, distance);
                std::swap(hash,  _table.hashes[i]);
                std::swap(key,   _table.keys[i]);
                std::swap(value, _table.values[i]);
                maxDistance = std::max(distance, maxDistance);
                distance = itsDistance;
            }
            ++distance;
        }

        _table.hashes[i] = hash;
        _table.keys[i]   = key;
        _table.values[i] = value;
        maxDistance = std::max(distance, maxDistance);
        _maxDistance = std::max(maxDistance, _maxDistance);
        //printf("    put '%.*s' at %zu (dist=%zd; max=%zd/%zu; load=%.2f)\n", FMTSLICE(key), i, distance, _maxDistance, _size, _count/double(_size));
    }


    void StringTable::table::allocate(size_t size) {
        size_t hashesSize = size * sizeof(hash_t),
               keysSize = size * sizeof(slice),
               valuesSize = size * sizeof(value_t);
        void *memory = ::malloc(hashesSize + keysSize + valuesSize);
        if (!memory)
            throw std::bad_alloc();
        hashes = (hash_t*)memory;
        keys   = (slice*)offsetby(hashes, hashesSize);
        values = (value_t*)offsetby(keys, keysSize);
        memset(hashes, 0, hashesSize);
    }


    void StringTable::allocTable(size_t size) {
        if (size <= kInitialTableSize) {
            size = kInitialTableSize;
            _table.hashes = _initialHashes;
            _table.keys   = _initialKeys;
            _table.values = _initialValues;
            ::memset(_initialHashes, 0, sizeof(_initialHashes));
        } else {
            _table.allocate(size);
        }
        _size = size;
        _sizeMask = size - 1;
        _maxDistance = 0;
        _maxCount = (size_t)(size * kMaxLoad);
    }


    void StringTable::grow() {
        table oldTable = _table;
        auto oldSize = _size;
        allocTable(2 * _size);
        for (size_t i = 0; i < oldSize; ++i) {
            if (oldTable.hashes[i])
                _add(oldTable.keys[i], oldTable.hashes[i], oldTable.values[i]);
        }
        if (oldTable.hashes != _initialHashes)
            oldTable.free();
    }


#if 0
    void StringTable::dump() const noexcept {
        int totalProbes = 0, maxProbes = 0;
        int n = 0;
        for (auto i = begin(); i != end(); ++i) {
            printf("%4d: ", n);
            slice key = **i;
            if (key) {
                size_t index = key.hash() & (_size - 1);
                int probes = 1 + n - (int)index;
                totalProbes += probes;
                maxProbes = std::max(maxProbes, probes);
                printf("(%4d) '%.*s'\n", probes, FMTSLICE(key));
            } else {
                printf("--\n");
            }
            ++n;
        }
        printf(">> Average number of probes = %.2f, max = %d", totalProbes/(double)count(), maxProbes);
    }
#endif

}
