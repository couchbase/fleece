//
// ConcurrentMap.hh
//
// Copyright © 2020 Couchbase. All rights reserved.
//

#pragma once
#include "PlatformCompat.hh"
#include "fleece/slice.hh"
#include <mutex>
#include <string>
#include <vector>

namespace fleece {

    class ConcurrentMap {
        public:
        ConcurrentMap(size_t capacity);
        ConcurrentMap(ConcurrentMap&&);
        ConcurrentMap& operator=(ConcurrentMap&&);
        ~ConcurrentMap();

        using value_t = uint32_t;

        enum class hash_t : uint32_t { Empty = 0 };


        struct entry_t {
            const char *key;
            value_t value;
            hash_t  hash = hash_t::Empty;

            slice keySlice() const                      {return slice(key, strlen(key));}
            bool hasKey(slice key) const FLPURE;
            __int128& as128() {return *(__int128*)this;}
            bool cas(entry_t expected, entry_t swapWith);
        };

        static inline hash_t hashCode(slice key) FLPURE {
            return hash_t( key.hash() );
        }

        size_t count() const FLPURE                     {return _count;}
        size_t capacity() const FLPURE                  {return _capacity;}
        size_t tableSize() const FLPURE                 {return _size;}

        entry_t find(slice key) const noexcept FLPURE   {return find(key, hashCode(key));}
        entry_t find(slice key, hash_t) const noexcept FLPURE;

        entry_t insert(slice key, value_t value)   {return insert(key, value, hashCode(key));}
        entry_t insert(slice key, value_t value, hash_t);

        size_t maxProbes() const;
        void dump() const;

    private:
        inline size_t wrap(size_t i) const              {return i & _sizeMask;}
        inline size_t indexOfHash(hash_t h) const       {return wrap(size_t(h));}

        size_t _size;           // Size of the table, always a power of 2
        size_t _sizeMask;       // _size-1, used for quick modulo: (i & _sizeMask) == (i % _size)
        size_t _count {0};
        size_t _capacity;
        entry_t* _entries {nullptr};      // Array of keys/values
        std::vector<alloc_slice> _keys;  // heap-allocated keys
        std::mutex _keysMutex;
    };

}
