//
// StringTable.cc
//
// Copyright 2015-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "StringTable.hh"
#include "PlatformCompat.hh"
#include <algorithm>
#include <stdlib.h>
#include <vector>
#include "betterassert.hh"

namespace fleece {

    // Minimum size [not capacity] of table to create initially
    static constexpr size_t kMinInitialSize = 16;

    // How full the table is allowed to get before it grows.
    // (Robin Hood hashing allows higher loads than regular hashing...)
    static const float kMaxLoad = 0.9f;


    StringTable::StringTable(size_t capacity)
    :StringTable(capacity, kMinInitialSize, nullptr, nullptr)
    { }


    StringTable::StringTable(size_t capacity,
                             size_t initialSize, hash_t *initialHashes, entry_t *initialEntries)
    {
        size_t size;
        for (size = initialSize; size * kMaxLoad < capacity; size *= 2)
            ;
        if (initialHashes && size <= initialSize)
            initTable(size, initialHashes, initialEntries);
        else
            allocTable(size);
    }


    StringTable::StringTable(const StringTable &s) {
        *this = s;
    }


    StringTable& StringTable::operator=(const StringTable &s) {
        if (_allocated) {
            free(_hashes);
            _hashes = nullptr;
        }
        _entries = nullptr;
        _allocated = false;

        allocTable(s._size);
        
        _count = s._count;
        _maxDistance = s._maxDistance;
        memcpy(_hashes, s._hashes, _count * sizeof(hash_t));
        memcpy(_entries, s._entries, _count * sizeof(entry_t));

        return *this;
    }


    StringTable::~StringTable() {
        if (_allocated)
            free(_hashes);
    }


    void StringTable::clear() noexcept {
        ::memset(_hashes, 0, _size * sizeof(hash_t));
        _count = 0;
        _maxDistance = 0;
    }


    __hot const StringTable::entry_t* StringTable::find(key_t key, hash_t hash) const noexcept {
        assert_precondition(key.buf != nullptr);
        assert_precondition(hash != hash_t::Empty);
        size_t end = wrap(size_t(hash) + _maxDistance + 1);
        for (size_t i = indexOfHash(hash); i != end; i = wrap(i + 1)) {
            if (_hashes[i] == hash_t::Empty)
                break;
            else if (_hashes[i] == hash && _entries[i].first == key)
                return &_entries[i];
        }
        return nullptr;
    }


    __hot StringTable::insertResult StringTable::insert(key_t key, value_t value, hash_t hash) {
        assert_precondition(key);
        assert_precondition(hash != hash_t::Empty);

        if (_usuallyFalse(_count > _capacity))
            grow();

        ssize_t distance = 0;
        auto maxDistance = _maxDistance;
        hash_t curHash = hash;
        entry_t curEntry = {key, value};
        entry_t *result = nullptr;
        // Walk along the table looking for an empty space:
        size_t i;
        for (i = indexOfHash(hash); _hashes[i] != hash_t::Empty; i = wrap(i + 1)) {
            assert(distance < _count);
            if (_hashes[i] == hash && _entries[i].first == key) {
                // Found the key in the table already:
                if (!result)
                    return {&_entries[i], false};   // Return existing entry
                else
                    break;
            }
            ssize_t itsDistance = wrap(i - indexOfHash(_hashes[i]) + _size);
            if (itsDistance < distance) {
                // Robin Hood hashing: Put new item where a less-distant existing item was:
                std::swap(curHash,  _hashes[i]);
                std::swap(curEntry, _entries[i]);
                maxDistance = std::max(distance, maxDistance);
                distance = itsDistance;
                if (!result)
                    result = &_entries[i];
                // Then continue, to find a new spot for the existing item we removed...
            }
            ++distance;
        }

        // Now place the final item in the empty space:
        _hashes[i]  = curHash;
        _entries[i] = std::move(curEntry);
        _maxDistance = std::max(distance, maxDistance);
        ++_count;

        if (!result)
            result = &_entries[i];
        return {result, true};                      // Return new entry
    }


    __hot void StringTable::insertOnly(key_t key, value_t value, hash_t hash) {
        assert_precondition(!find(key, hash));
        if (_usuallyFalse(++_count > _capacity))
            grow();
        _insertOnly(hash, {key, value});
    }


    // Subroutine of insertOnly() and grow() that doesn't bump count or grow table.
    // This repeats a lot of the logic of insert(), but I decided performance trumps DRY.
    __hot void StringTable::_insertOnly(hash_t hash, entry_t entry) noexcept {
        assert_precondition(entry.first);
        assert_precondition(hash != hash_t::Empty);
        ssize_t distance = 0;
        auto maxDistance = _maxDistance;
        // Walk along the table looking for an empty space:
        size_t i;
        for (i = indexOfHash(hash); _hashes[i] != hash_t::Empty; i = wrap(i + 1)) {
            assert(distance < _count);
            ssize_t itsDistance = wrap(i - indexOfHash(_hashes[i]) + _size);
            if (itsDistance < distance) {
                // Robin Hood hashing: Put new item where a less-distant existing item was:
                std::swap(hash,  _hashes[i]);
                std::swap(entry, _entries[i]);
                maxDistance = std::max(distance, maxDistance);
                distance = itsDistance;
                // Then continue, to find a new spot for the existing item we removed...
            }
            ++distance;
        }

        // Now place the final item in the empty space:
        _hashes[i]  = hash;
        _entries[i] = std::move(entry);
        _maxDistance = std::max(distance, maxDistance);
    }


#pragma mark - TABLE ALLOCATION:


    void StringTable::initTable(size_t size, hash_t *hashes, entry_t *entries) {
        _size = size;
        _sizeMask = size - 1;
        _maxDistance = 0;
        _capacity = (size_t)(size * kMaxLoad);
        _hashes = hashes;
        _entries = entries;
        memset(_hashes, 0, size * sizeof(hash_t));
    }


    void StringTable::allocTable(size_t size) {
        size_t hashesSize  = size * sizeof(hash_t), entriesSize = size * sizeof(entry_t);
        void *memory = ::malloc(hashesSize + entriesSize);
        if (!memory)
            throw std::bad_alloc();
        initTable(size, (hash_t*)memory, (entry_t*)offsetby(memory, hashesSize));
        _allocated = true;
    }


    __hot void StringTable::grow() {
        auto oldSize = _size;
        auto oldHashes = _hashes;
        auto oldEntries = _entries;
        auto wasAllocated = _allocated;

        allocTable(2 * oldSize);

        for (size_t i = 0; i < oldSize; ++i) {
            if (oldHashes[i] != hash_t::Empty)
                _insertOnly(oldHashes[i], oldEntries[i]);
        }
        if (wasAllocated)
            free(oldHashes);
    }


    void StringTable::dump() const noexcept {
        ssize_t totalDistance = 0;
        std::vector<size_t> distanceCounts(_maxDistance+1);
        for (size_t i = 0; i < _size; ++i) {
            printf("%4zd: ", i);
            if (_hashes[i] != hash_t::Empty) {
                key_t key = _entries[i].first;
                size_t index = indexOfHash(hashCode(key));
                ssize_t distance = (i - (int)index + _size) & _sizeMask;
                totalDistance += distance;
                ++distanceCounts[distance];
                printf("(%2zd) '%.*s'\n", distance, FMTSLICE(key));
            } else {
                printf("--\n");
            }
        }
        printf(">> Capacity %zd, using %zu (%.0f%%)\n",
               _size, _count,  _count/(double)_size*100.0);
        printf(">> Average key distance = %.2f, max = %zd\n",
               totalDistance/(double)count(), _maxDistance);
        for (decltype(_maxDistance) i = 0; i <= _maxDistance; ++i)
            printf("\t%2zd: %zd\n", i, distanceCounts[i]);
    }

}
