//
// ConcurrentMap.hh
//
// Copyright © 2020 Couchbase. All rights reserved.
//

#pragma once
#include "ConcurrentArena.hh"
#include "fleece/slice.hh"
#include <atomic>
#include <memory>

namespace fleece {

    /** An (almost-)lockless concurrent hash table that maps strings to 32-bit ints.
        Does not support deletion. Intended for use by SharedKeys. */
    class ConcurrentMap {
        public:
        static constexpr int kMaxCapacity = 0x7FFF;
        static constexpr int kMaxStringCapacity = 0x10000;

        ConcurrentMap(int capacity, int stringCapacity =0);

        // Move cannot be concurrent with find or insert calls!
        ConcurrentMap(ConcurrentMap&&);
        ConcurrentMap& operator=(ConcurrentMap&&);

        using value_t = uint16_t;

        enum class hash_t : uint32_t { };

        /** Computes the hash code of a key. This code can be passed to alternate versions of the
            `find` and `insert` methods, to avoid hashing the same key multiple times. */
        static inline hash_t hashCode(slice key) FLPURE {return hash_t( key.hash() );}

        int count() const FLPURE                     {return _count;}
        int capacity() const FLPURE                  {return _capacity;}
        int tableSize() const FLPURE                 {return _sizeMask + 1;}
        int stringBytesCapacity() const FLPURE       {return (int)_keys.capacity();}
        int stringBytesCount() const FLPURE          {return (int)_keys.allocated();}

        struct result {
            slice key;
            uint16_t value;
        };

        /** Looks up the key. Returns the value, as well as the key in memory owned by the map.
            If the key is not found, returns a result with an empty slice for the key. */
        result find(slice key) const noexcept FLPURE    {return find(key, hashCode(key));}
        result find(slice key, hash_t) const noexcept FLPURE;

        /** Inserts a value for a key. Returns the value, as well as a new copy of the key in memory owned
            by the map.
            If the key already exists, the existing value is not changed, and the existing value is
            returned as well as the managed copy of the key (as from `find`.)
            If the hash table or key storage is full and the key can't be inserted, returns an empty
            slice. */
        result insert(slice key, value_t value)         {return insert(key, value, hashCode(key));}
        result insert(slice key, value_t value, hash_t);

        void dump() const;

    private:
        struct Entry {
            uint16_t keyOffset;               // 1 + offset of key in _keys, or 0 if empty
            uint16_t value;                   // value of key

            uint32_t& asInt32()                 {return *(uint32_t*)this;}
            bool compareAndSwap(Entry expected, Entry swapWith);
        };

        inline int wrap(int i) const            {return i & _sizeMask;}
        inline int indexOfHash(hash_t h) const  {return wrap(int(h));}
        const char* allocKey(slice key);
        bool freeKey(const char *allocedKey);

        const char* entryKey(Entry entry) const {
            assert(entry.keyOffset > 0);
            return (const char*)_keys.toPointer(entry.keyOffset - 1);
        }

        int                      _sizeMask;   // table size - 1; used for quick modulo via AND
        int                      _capacity;   // Max number of entries
        std::atomic<int>         _count {0};  // Current number of entries
        std::unique_ptr<Entry[]> _entries;    // The table: array of key/value pairs
        ConcurrentArena          _keys;       // Key storage
    };

}
