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

#include "fleece/slice.hh"
#include <algorithm>

namespace fleece {

    /** Internal hash table mapping strings (slices) to offsets (uint32_t). */
    class StringTable {
    public:
        StringTable(size_t capacity =0);
        ~StringTable();

        using hash_t = uint32_t;
        using value_t = uint32_t;

        static inline hash_t hashCode(slice key) {
            return std::max(key.hash(), 1u);
        }

        size_t count() const                        {return _count;}
        size_t tableSize() const                    {return _size;}

        void clear() noexcept;

        value_t* get(slice key) const noexcept      {return get(key, hashCode(key));}
        value_t* get(slice key, hash_t) const noexcept;

        using position_t = ssize_t;
        static constexpr position_t kNoPosition = -1;

        position_t find(slice key) const noexcept   {return find(key, hashCode(key));}
        position_t find(slice key, hash_t) const noexcept;

        bool exists(position_t pos) const               {return _table.hashes[pos] != 0;}
        slice keyAt(position_t pos) const               {return _table.keys[pos];}
        value_t valueAt(position_t pos) const           {return _table.values[pos];}
        void setValueAt(position_t pos, value_t val)    {_table.values[pos] = val;}

        void add(slice key, value_t value)          {return add(key, hashCode(key), value);}
        void add(slice, hash_t, value_t);

        void dump() const noexcept;

    private:
        void _add(slice, hash_t, value_t) noexcept;
        void allocTable(size_t size);
        void grow();

        static const size_t kInitialTableSize = 64;

        size_t _size, _sizeMask;
        size_t _count;
        size_t _maxCount;
        ssize_t _maxDistance;

        struct table {
            hash_t*  hashes = nullptr;
            slice*   keys = nullptr;
            value_t* values = nullptr;

            void allocate(size_t size);
            void free() noexcept                        {::free(hashes);}
        };

        table _table;

        hash_t  _initialHashes[kInitialTableSize];
        slice   _initialKeys[kInitialTableSize];
        value_t _initialValues[kInitialTableSize];
    };

}
