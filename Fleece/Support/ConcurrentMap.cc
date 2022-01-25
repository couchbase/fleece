//
// ConcurrentMap.cc
//
// Copyright 2020-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "ConcurrentMap.hh"
#include <algorithm>
#include <cmath>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif


using namespace std;

namespace fleece {

    /*
     This is based on the “folklore” table described in “Concurrent Hash Tables: Fast and
     General(?)!” by Maier et al. <https://arxiv.org/pdf/1601.04017.pdf>. It's a pretty basic
     open hash table with linear probing. Atomic compare-and-swap operations are used to update
     entries, but not to read them.

     It doesn't support modifying the value of an entry, simply because SharedKeys doesn't need it.

     It cannot grow past its initial capacity; this is rather difficult to do in a concurrent map
     (the paper describes how) and would add a lot of complexity ... and again, SharedKeys has a
     fixed capacity of 2048 so it doesn't need this.

     Since insertions are not very common, it's worth the expense to materialize the count in an
     atomic integer variable, and update it on insert/delete, instead of the more complex
     techniques used in the paper.

     Maier et al state (without explanation) that, unlike ordinary open hash tables, the "folklore"
     table cannot reuse 'tombstones' for new entries. I believe the reason is that there could be
     incorrect results from "torn reads" -- non-atomic reads where the two fields of the Entry
     are not consistent with each other. However, this implementation does not suffer from torn
     reads since its Entry is only 32 bits, as opposed to two words (128 bits). So to the best
     of my knowledge, reusing deleted entries is safe.

     Still, this table does have the common problem that large numbers of tombstones degrade
     read performance, since tombstones have to be scanned past by the `find` method just as if they
     were occupied. The solution would be to copy the extant entries to a new table, but (as with
     growing) this is pretty complex in a lockless concurrent table.
     */


    // Minimum size [not capacity] of table to create initially
    static constexpr size_t kMinInitialSize = 16;

    // Max fraction of table entries that should be occupied (else lookups slow down)
    static constexpr float kMaxLoad = 0.6f;


    // Special values of Entry::keyOffset
    static constexpr uint16_t kEmptyKeyOffset = 0,   // an empty Entry
                              kDeletedKeyOffset = 1, // a deleted Entry
                              kMinKeyOffset = 2;     // first actual key offset


    // Cross-platform atomic test-and-set primitive:
    // If `*value == oldValue`, stores `newValue` and returns true. Else returns false.
    // Note: This could work with values up to 128 bits, on current 64-bit CPUs.
    static inline bool atomicCompareAndSwap(uint32_t *value, uint32_t oldValue, uint32_t newValue) {
#if defined(_WIN32) || defined(_WIN64)
        // https://docs.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-interlockedcompareexchange
        return InterlockedCompareExchange(value, newValue, oldValue) == oldValue;
#else
        // https://gcc.gnu.org/onlinedocs/gcc-4.1.1/gcc/Atomic-Builtins.html  (also in Clang)
        return __sync_bool_compare_and_swap(value, oldValue, newValue);
#endif
    }



    ConcurrentMap::ConcurrentMap(int capacity, int stringCapacity) {
        precondition(capacity <= kMaxCapacity);
        int size;
        for (size = kMinInitialSize; size * kMaxLoad < capacity; size *= 2)
            ;
        _capacity = int(floor(size * kMaxLoad));
        _sizeMask = size - 1;

        if (stringCapacity == 0)
            stringCapacity = 17 * _capacity;    // assume 16-byte strings by default
        stringCapacity = min(stringCapacity, int(kMaxStringCapacity));
        size_t tableSize = size * sizeof(Entry);
        
        _heap = ConcurrentArena(tableSize + stringCapacity);
        _entries = ConcurrentArenaAllocator<Entry, true>(_heap).allocate(size);
        _keysOffset = tableSize - kMinKeyOffset;
        
        postcondition(stringCapacity >= 0 && _heap.available() == size_t(stringCapacity));
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
        return int(_heap.capacity() - (_keysOffset + kMinKeyOffset));
    }


    int ConcurrentMap::stringBytesCount() const {
        return int(_heap.allocated() - (_keysOffset + kMinKeyOffset));
    }


    __hot
    bool ConcurrentMap::Entry::compareAndSwap(Entry expected, Entry swapWith) {
        static_assert(sizeof(Entry) == 4);
        return atomicCompareAndSwap(&asInt32(), expected.asInt32(), swapWith.asInt32());
    }


    __hot FLPURE
    static inline bool equalKeys(const char *keyPtr, slice key) {
        return memcmp(keyPtr, key.buf, key.size) == 0 && keyPtr[key.size] == 0;
    }


    __hot
    inline uint16_t ConcurrentMap::keyToOffset(const char *allocedKey) const {
        ptrdiff_t result = _heap.toOffset(allocedKey) - _keysOffset;
        assert(result >= kMinKeyOffset && result <= UINT16_MAX);
        return uint16_t(result);
    }


    __hot
    inline const char* ConcurrentMap::offsetToKey(uint16_t offset) const {
        assert(offset >= kMinKeyOffset);
        return (const char*)_heap.toPointer(_keysOffset + offset);
    }


    __hot
    ConcurrentMap::result ConcurrentMap::find(slice key, hash_t hash) const noexcept {
        assert_precondition(key);
        for (int i = indexOfHash(hash); true; i = wrap(i + 1)) {
            Entry current = _entries[i];
            switch (current.keyOffset) {
                case kEmptyKeyOffset:
                    return {};
                case kDeletedKeyOffset:
                    break;
                default:
                    if (auto keyPtr = offsetToKey(current.keyOffset); equalKeys(keyPtr, key))
                        return {slice(keyPtr, key.size), current.value};
                    break;
            }
        }
    }


    __hot
    ConcurrentMap::result ConcurrentMap::insert(slice key, value_t value, hash_t hash) {
        assert_precondition(key);
        const char *allocedKey = nullptr;
        int i = indexOfHash(hash);
        while (true) {
        retry:
            Entry current = _entries[i];
            switch (current.keyOffset) {
                case kEmptyKeyOffset:
                case kDeletedKeyOffset: {
                    // Found an empty or deleted entry to use. First allocate the string:
                    if (!allocedKey) {
                        if (_count >= _capacity)
                            return {};          // Hash table overflow
                        allocedKey = allocKey(key);
                        if (!allocedKey)
                            return {};          // Key-strings overflow
                    }
                    Entry newEntry = {keyToOffset(allocedKey), value};
                    // Try to store my new entry, if another thread didn't beat me to it:
                    if (_usuallyFalse(!_entries[i].compareAndSwap(current, newEntry))) {
                        // I was beaten to it; retry (at the same index,
                        // in case CAS was a false negative)
                        goto retry;
                    }
                    // Success!
                    ++_count;
                    assert(_count <= _capacity);
                    return {slice(allocedKey, key.size), value};
                }
                default:
                    if (auto keyPtr = offsetToKey(current.keyOffset); equalKeys(keyPtr, key)) {
                        // Key already exists in table. Deallocate any string I allocated:
                        freeKey(allocedKey);
                        return {slice(keyPtr, key.size), current.value};
                    }
                    break;
            }
            i = wrap(i + 1);
        }
    }


    bool ConcurrentMap::remove(slice key, hash_t hash) {
        assert_precondition(key);
        int i = indexOfHash(hash);
        while (true) {
        retry:
            Entry current = _entries[i];
            switch (current.keyOffset) {
                case kEmptyKeyOffset:
                    // Not found.
                    return false;
                case kDeletedKeyOffset:
                    break;
                default:
                    if (auto keyPtr = offsetToKey(current.keyOffset); equalKeys(keyPtr, key)) {
                        // Found it -- now replace with a tombstone. Leave the value alone in case
                        // a concurrent torn read sees the prior offset + new value.
                        Entry tombstone = {kDeletedKeyOffset, current.value};
                        if (_usuallyFalse(!_entries[i].compareAndSwap(current, tombstone))) {
                            // I was beaten to it; retry (at the same index,
                            // in case CAS was a false negative)
                            goto retry;
                        }
                        // Success!
                        --_count;
                        // Freeing the key string will only do anything if it was the latest key
                        // to be added, but it's worth a try.
                        (void)freeKey(keyPtr);
                        return true;
                    }
                    break;
            }
            i = wrap(i + 1);
        }
    }


    const char* ConcurrentMap::allocKey(slice key) {
        auto result = (char*)_heap.alloc(key.size + 1);
        if (result) {
            key.copyTo(result);
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
        int realCount = 0, tombstones = 0, totalDistance = 0, maxDistance = 0;
        for (int i = 0; i < size; i++) {
            auto e = _entries[i];
            switch (e.keyOffset) {
                case kEmptyKeyOffset:
                    printf("%6d\n", i);
                    break;
                case kDeletedKeyOffset:
                    ++tombstones;
                    printf("%6d xxx\n", i);
                    break;
                default: {
                    ++realCount;
                    auto keyPtr = offsetToKey(e.keyOffset);
                    hash_t hash = hashCode(slice(keyPtr));
                    int bestIndex = indexOfHash(hash);
                    printf("%6d: %-10s = %08x [%5d]", i, keyPtr, (uint32_t)hash, bestIndex);
                    if (i != bestIndex) {
                        if (bestIndex > i)
                            bestIndex -= size;
                        auto distance = i - bestIndex;
                        printf(" +%d", distance);
                        totalDistance += distance;
                        maxDistance = max(maxDistance, distance);
                    }
                    printf("\n");
                }
            }
        }
        printf("Occupancy = %d / %d (%.0f%%), with %d tombstones\n",
               realCount, size, realCount/double(size)*100.0, tombstones);
        printf("Average probes = %.1f, max probes = %d\n",
               1.0 + (totalDistance / (double)realCount), maxDistance);
    }

}
