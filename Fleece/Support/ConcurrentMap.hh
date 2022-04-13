//
// ConcurrentMap.hh
//
// Copyright 2020-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "ConcurrentArena.hh"
#include "fleece/slice.hh"
#include <atomic>
#include <cstdint>
#include <memory>

namespace fleece {

    /** A lockless concurrent hash table that maps strings to 16-bit ints.
        Intended for use by SharedKeys. */
    class ConcurrentMap {
        public:
        static constexpr int kMaxCapacity = 0x7FFF;
        static constexpr int kMaxStringCapacity = 0x10000;

        /** Constructs a ConcurrentMap. The capacity is fixed.
            @param capacity  The number of keys it needs to hold. Cannot exceed kMaxCapacity.
            @param stringCapacity  Maximum total size in bytes of all keys, including one byte per
                                   key as a separator. Cannot exceed kMaxStringCapacity.
                                   If 0 or omitted, value is `17 * capacity`. */
        ConcurrentMap(int capacity, int stringCapacity =0);

        // Move cannot be concurrent with find or insert calls!
        ConcurrentMap(ConcurrentMap&&);
        ConcurrentMap& operator=(ConcurrentMap&&);

        /// The type of value associated with a key.
        using value_t = uint16_t;

        /// The hash code of a key.
        enum class hash_t : uint32_t { };

        /** Computes the hash code of a key. This code can be passed to alternate versions of the
            `find` and `insert` methods, to avoid hashing the same key multiple times. */
        static inline hash_t hashCode(slice key) FLPURE {return hash_t( key.hash() );}

        int count() const FLPURE                     {return _count;}
        int capacity() const FLPURE                  {return _capacity;}
        int tableSize() const FLPURE                 {return _sizeMask + 1;}
        int stringBytesCapacity() const FLPURE;
        int stringBytesCount() const FLPURE;

        struct result {
            slice key;
            uint16_t value;
        };

        /** Looks up the key. Returns the value, as well as the key in memory owned by the map
            (which is guaranteed to remain valid until the entry is removed or the map destructed.)
            If the key is not found, returns a result with an empty slice for the key. */
        result find(slice key) const noexcept FLPURE    {return find(key, hashCode(key));}
        result find(slice key, hash_t) const noexcept FLPURE;

        /** Inserts a value for a key. Returns the value, as well as a new copy of the key in
            memory owned by the map.
            If the key already exists, the existing value is not changed, and the existing value is
            returned as well as the managed copy of the key (as from `find`.)
            If the hash table or key storage is full and the key can't be inserted, returns an empty
            slice. */
        result insert(slice key, value_t value)         {return insert(key, value, hashCode(key));}
        result insert(slice key, value_t value, hash_t);

        /** Removes the value for a key. Returns true if removed, false if not found.
            \note  The space occupied by the key string can only be recovered if this was the
                   last key added, so when removing multiple keys it's best to go in reverse
                   chronological order. */
        bool remove(slice key)                          {return remove(key, hashCode(key));}
        bool remove(slice key, hash_t hash);

        /** Writes all the table entries, plus statistics, to stdout. */
        void dump() const;

    private:
        // Hash table entry (32 bits).
        struct alignas(uint32_t) Entry {
            uint16_t keyOffset;     // offset of key from _keysOffset, or 0 if empty, 1 if deleted
            uint16_t value;         // value associated with key

            uint32_t& asInt32()                 {return *(uint32_t*)this;}
            bool compareAndSwap(Entry expected, Entry swapWith);
        };

        inline int wrap(int i) const            {return i & _sizeMask;}
        inline int indexOfHash(hash_t h) const  {return wrap(int(h));}
        const char* allocKey(slice key);
        bool freeKey(const char *allocedKey);

        inline uint16_t keyToOffset(const char *allocedKey) const FLPURE;
        inline const char* offsetToKey(uint16_t offset) const FLPURE;

        int                 _sizeMask;   // table size - 1; used for quick modulo via AND
        int                 _capacity;   // Max number of entries
        std::atomic<int>    _count {0};  // Current number of entries
        ConcurrentArena     _heap;       // Storage for entries + keys
        Entry*              _entries;    // The table: array of key/value pairs
        size_t              _keysOffset; // Start of key storage
    };

}
