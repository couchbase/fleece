//
// StringTable.hh
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

#pragma once

#include "PlatformCompat.hh"
#include "fleece/slice.hh"
#include <algorithm>

namespace fleece {

    /** Internal hash table mapping strings (slices) to integers (uint32_t). */
    class StringTable {
    public:
        StringTable(size_t capacity =0);
        ~StringTable();

        using key_t   = slice;
        using value_t = uint32_t;

        using entry_t = std::pair<key_t, value_t>;

        enum class hash_t : uint32_t { Empty = 0 };

        static inline hash_t hashCode(key_t key) PURE {
            return hash_t( std::max(key.hash(), 1u) ); // hashCode must never be zero
        }

        size_t count() const PURE                            {return _count;}
        size_t tableSize() const PURE                        {return _size;}

        void clear() noexcept;

        /// Looks up an existing key, returning a pointer to its entry (or NULL.)
        const entry_t* find(key_t key) const noexcept PURE   {return find(key, hashCode(key));}
        const entry_t* find(key_t key, hash_t) const noexcept PURE;

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

        StringTable(const StringTable&) =delete;
        StringTable& operator=(const StringTable&) =delete;

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
