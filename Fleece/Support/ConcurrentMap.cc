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
#include <cmath>

using namespace std;

namespace fleece {


    // Minimum size [not capacity] of table to create initially
    static constexpr size_t kMinInitialSize = 16;

    // How full the table is allowed to get before it grows.
    static constexpr float kMaxLoad = 0.6f;


    ConcurrentMap::ConcurrentMap(int capacity, int stringCapacity) {
        precondition(capacity <= kMaxCapacity);
        int size;
        for (size = kMinInitialSize; size * kMaxLoad < capacity; size *= 2)
            ;
        _capacity = int(floor(size * kMaxLoad));
        _sizeMask = size - 1;

        if (stringCapacity == 0)
            stringCapacity = 17 * _capacity;
        stringCapacity = min(stringCapacity, int(kMaxStringCapacity));
        size_t tableSize = size * sizeof(Entry);
        
        _heap = ConcurrentArena(tableSize + stringCapacity);
        _entries = ConcurrentArenaAllocator<Entry, true>(_heap).allocate(size);
        _keysOffset = tableSize;
        
        assert(_heap.available() == stringCapacity);
    }


    ConcurrentMap::ConcurrentMap(ConcurrentMap &&map) {
        *this = move(map);
    }


    ConcurrentMap& ConcurrentMap::operator=(ConcurrentMap &&map) {
        _sizeMask = map._sizeMask;
        _capacity = map._capacity;
        _count = map._count.load();
        _entries = map._entries;
        _heap = move(map._heap);
        return *this;
    }


    int ConcurrentMap::stringBytesCapacity() const {
        return int(_heap.capacity() - _keysOffset);
    }


    int ConcurrentMap::stringBytesCount() const {
        return int(_heap.allocated() - _keysOffset);
    }


    bool ConcurrentMap::Entry::compareAndSwap(Entry expected, Entry swapWith) {
        // https://gcc.gnu.org/onlinedocs/gcc-4.1.1/gcc/Atomic-Builtins.html
        static_assert(sizeof(Entry) == 4);
        return __sync_bool_compare_and_swap_4(&asInt32(), expected.asInt32(), swapWith.asInt32());
    }


    __hot
    static inline bool equalKeys(const char *keyPtr, slice key) {
        return memcmp(keyPtr, key.buf, key.size) == 0 && keyPtr[key.size] == 0;
    }


    __hot
    inline uint16_t ConcurrentMap::keyToOffset(const char *allocedKey) const {
        auto off = _heap.toOffset(allocedKey);
        assert(off >= _keysOffset && off < _keysOffset + 65535);
        return uint16_t(off - _keysOffset + 1);
    }


    __hot
    inline const char* ConcurrentMap::offsetToKey(uint16_t offset) const {
        assert(offset > 0);
        return (const char*)_heap.toPointer(_keysOffset + offset - 1);
    }


    __hot
    ConcurrentMap::result ConcurrentMap::find(slice key, hash_t hash) const noexcept {
        assert_precondition(key);
        for (int i = indexOfHash(hash); true; i = wrap(i + 1)) {
            Entry current = _entries[i];
            if (current.keyOffset == 0)
                return {};
            else if (auto keyPtr = offsetToKey(current.keyOffset); equalKeys(keyPtr, key))
                return {slice(keyPtr, key.size), current.value};
        }
    }


    ConcurrentMap::result ConcurrentMap::insert(slice key, value_t value, hash_t hash) {
        assert_precondition(key);
        const char *allocedKey = nullptr;
        int i = indexOfHash(hash);
        while (true) {
            Entry current = _entries[i];
            if (current.keyOffset == 0) {
                // Found an empty entry to store the key in. First allocate the string:
                if (!allocedKey) {
                    if (_count >= _capacity)
                        return {};          // Hash table overflow
                    allocedKey = allocKey(key);
                    if (!allocedKey)
                        return {};          // Key-strings overflow
                }
                Entry newEntry = {keyToOffset(allocedKey), value};
                // Try to store my new entry, if another thread didn't beat me to it:
                if (_entries[i].compareAndSwap(current, newEntry)) {
                    // Success!
                    ++_count;
                    assert(_count <= _capacity);//TEMP
                    return {slice(allocedKey, key.size), value};
                } else {
                    // I was beaten to it; retry (at the same index, in case CAS was false positive)
                    continue;
                }
            } else if (auto keyPtr = offsetToKey(current.keyOffset); equalKeys(keyPtr, key)) {
                // Key already exists in table. Deallocate any string I allocated:
                freeKey(allocedKey);
                return {slice(keyPtr, key.size), current.value};
            }
            i = wrap(i + 1);
        }
    }


    const char* ConcurrentMap::allocKey(slice key) {
        auto result = (char*)_heap.alloc(key.size + 1);
        if (result) {
            memcpy(result, key.buf, key.size);
            result[key.size] = 0;
        }
        return result;
    }


    bool ConcurrentMap::freeKey(const char *allocedKey) {
        return allocedKey == nullptr || _heap.free((void*)allocedKey, strlen(allocedKey) + 1);
    }

    __cold
    void ConcurrentMap::dump() const {
        int size = tableSize();
        int realCount = 0, totalDistance = 0, maxDistance = 0;
        for (int i = 0; i < size; i++) {
            if (auto e = _entries[i]; e.keyOffset != 0) {
                ++realCount;
                auto keyPtr = offsetToKey(e.keyOffset);
                hash_t hash = hashCode(slice(keyPtr));
                int bestIndex = indexOfHash(hash);
                printf("%6d: %-10s = %08x [%5d]", i, keyPtr, hash, bestIndex);
                if (i != bestIndex) {
                    if (bestIndex > i)
                        bestIndex -= size;
                    auto distance = i - bestIndex;
                    printf(" +%d", distance);
                    totalDistance += distance;
                    maxDistance = max(maxDistance, distance);
                }
                printf("\n");
            } else {
                printf("%6d\n", i);
            }
        }
        printf("Occupancy = %d / %d (%.0f%%)\n", realCount, size, realCount/double(size)*100.0);
        printf("Average probes = %.1f, max probes = %d\n",
               1.0 + (totalDistance / (double)realCount), maxDistance);
    }

}
