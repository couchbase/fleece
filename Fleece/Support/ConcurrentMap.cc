//
// ConcurrentMap.cc
//
// Copyright © 2020 Couchbase. All rights reserved.
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

#include "ConcurrentMap.hh"

using namespace std;

namespace fleece {


    // Minimum size [not capacity] of table to create initially
    static constexpr size_t kMinInitialSize = 16;

    // How full the table is allowed to get before it grows.
    // (Robin Hood hashing allows higher loads than regular hashing...)
    static constexpr float kMaxLoad = 0.6f;


    ConcurrentMap::ConcurrentMap(size_t capacity) {
        for (_size = kMinInitialSize; _size * kMaxLoad < capacity; _size *= 2)
            ;
        _capacity = size_t(_size * kMaxLoad);
        _sizeMask = _size - 1;
        _entries = new entry_t[_size];
        memset(_entries, 0, _size * sizeof(entry_t));
    }


    ConcurrentMap::ConcurrentMap(ConcurrentMap &&map) {
        *this = move(map);
    }


    ConcurrentMap& ConcurrentMap::operator=(ConcurrentMap &&map) {
        delete [] _entries;
        _size = map._size;
        _sizeMask = map._sizeMask;
        _count = map._count;
        _capacity = map._capacity;
        _entries = map._entries;
        _keys = move(map._keys);
        map._entries = nullptr;
        return *this;
    }


    ConcurrentMap::~ConcurrentMap() {
        delete [] _entries;
    }


    __hot ConcurrentMap::entry_t ConcurrentMap::find(slice key, hash_t hash) const noexcept {
        assert_precondition(key);
        for (size_t i = indexOfHash(hash); true; i = wrap(i + 1)) {
            entry_t current = _entries[i];
            if (current.key == nullptr)
                return {};
            else if (current.hash == hash && current.hasKey(key))
                return current;
        }
    }


    bool ConcurrentMap::entry_t::hasKey(slice keySlice) const {
        return memcmp(key, keySlice.buf, keySlice.size) == 0 && key[keySlice.size] == 0;
    }


    bool ConcurrentMap::entry_t::cas(entry_t expected, entry_t swapWith) {
        // https://gcc.gnu.org/onlinedocs/gcc-4.1.1/gcc/Atomic-Builtins.html
        return __sync_bool_compare_and_swap_16(&as128(),
                                               expected.as128(), swapWith.as128());
    }


    ConcurrentMap::entry_t ConcurrentMap::insert(slice key, value_t value, hash_t hash) {
        assert_precondition(key);
        alloc_slice allocedKey;
        size_t i = indexOfHash(hash);
        while (true) {
            entry_t current = _entries[i];
            if (current.key == nullptr) {
                if (!allocedKey)
                    allocedKey = alloc_slice::nullPaddedString(key);
                entry_t newEntry = {(const char*)allocedKey.buf, value, hash};
                if (_entries[i].cas(current, newEntry)) {
                    lock_guard<mutex> lock(_keysMutex);
                    if (_count >= _capacity)
                        return {};          // Overflow!
                    ++_count;
                    _keys.push_back(move(allocedKey));
                    return newEntry;
                } else {
                    continue; // retry at same index
                }
            } else if (current.hash == hash && current.hasKey(key)) {
                return current;
            }
            i = wrap(i + 1);
        }
    }


    size_t ConcurrentMap::maxProbes() const {
        size_t maxp = 0;
        for (size_t i = 0; i < _size; i++) {
            if (auto e = _entries[i]; e.key != nullptr) {
                size_t bestIndex = indexOfHash(e.hash);
                if (bestIndex > i)
                    bestIndex -= _size;
                maxp = max(maxp, i - bestIndex);
            }
        }
        return maxp;
    }


    void ConcurrentMap::dump() const {
        for (size_t i = 0; i < _size; i++) {
            if (auto e = _entries[i]; e.key != nullptr) {
                size_t bestIndex = indexOfHash(e.hash);
                printf("%6zu: %-10s = %08x [%5zu]", i, e.key, e.hash, bestIndex);
                if (i != bestIndex) {
                    if (bestIndex > i)
                        bestIndex -= _size;
                    printf(" +%lu", i - bestIndex);
                }
                printf("\n");
            } else {
                printf("%6zu\n", i);
            }
        }
    }

}
