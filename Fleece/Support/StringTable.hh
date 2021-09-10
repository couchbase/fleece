//
// StringTable.hh
//
// Copyright 2015-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once

#include "PlatformCompat.hh"
#include "fleece/slice.hh"
#include <algorithm>
#include <utility>

namespace fleece {

    /** Internal hash table mapping strings (slices) to integers (uint32_t). */
    class StringTable {
    public:
        StringTable(size_t capacity =0);
        StringTable(const StringTable&);
        StringTable& operator=(const StringTable&);
        ~StringTable();

        using key_t   = slice;
        using value_t = uint32_t;

        using entry_t = std::pair<key_t, value_t>;

        enum class hash_t : uint32_t { Empty = 0 };

        static inline hash_t hashCode(key_t key) FLPURE {
            return hash_t( std::max(key.hash(), 1u) ); // hashCode must never be zero
        }

        size_t count() const FLPURE                            {return _count;}
        size_t tableSize() const FLPURE                        {return _size;}

        void clear() noexcept;

        /// Looks up an existing key, returning a pointer to its entry (or NULL.)
        const entry_t* find(key_t key) const noexcept FLPURE   {return find(key, hashCode(key));}
        const entry_t* find(key_t key, hash_t) const noexcept FLPURE;

        using insertResult = std::pair<entry_t*, bool>;

        /// Adds a key and its value, or finds an existing key (and doesn't change it).
        /// Returns a reference to the entry, and a flag that's true if it's newly added.
        insertResult insert(key_t key, value_t value)   {return insert(key, value, hashCode(key));}
        insertResult insert(key_t key, value_t value, hash_t);

        /// Faster version of \ref insert that only inserts new keys.
        /// MUST NOT be called if the key already exists in the table!
        void insertOnly(key_t key, value_t value)       {insertOnly(key, value, hashCode(key));}
        void insertOnly(key_t key, value_t value, hash_t);

        void dump() const noexcept;

    protected:
        StringTable(size_t capacity,
                    size_t initialSize, hash_t *initialHashes, entry_t *initialEntries);
        inline size_t wrap(size_t i) const              {return i & _sizeMask;}
        inline size_t indexOfHash(hash_t h) const       {return wrap(size_t(h));}
        insertResult _insert(hash_t, entry_t) noexcept;
        void _insertOnly(hash_t, entry_t) noexcept;
        void allocTable(size_t size);
        void grow();
        void initTable(size_t size, hash_t *hashes, entry_t *entries);

        size_t _size;           // Size of the arrays
        size_t _sizeMask;       // Used for quick modulo: (i & _sizeMask) == (i % _size)
        size_t _count {0};      // Number of entries
        size_t _capacity;       // Grow the table when it exceeds this count
        ssize_t _maxDistance;   // Maximum distance of any entry from its ideal position
        hash_t*  _hashes;       // Array of hash codes, zero meaning empty
        entry_t* _entries;      // Array of keys/values, paralleling _hashes
        bool _allocated {false};// Was table allocated by allocTable?
    };



    /** A StringTable with extra space at the end for the initial table, thus saving a malloc
        if the initial capacity fits into that size. */
    template <size_t INITIAL_SIZE =32>
    class PreallocatedStringTable : public StringTable {
    public:
        PreallocatedStringTable(size_t capacity =0)
        :StringTable(capacity, INITIAL_SIZE, _initialHashes, _initialEntries)
        { }

    private:
        hash_t  _initialHashes[INITIAL_SIZE];
        entry_t _initialEntries[INITIAL_SIZE];
    };

}
