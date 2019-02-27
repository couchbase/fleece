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

namespace fleece {

    /** Internal hash table mapping strings (slices) to offsets (uint32_t). */
    class StringTable {
    public:
        StringTable(size_t capacity =0);
        ~StringTable();

        struct info {
            uint32_t offset;            // Used by clients (Encoder, SharedKeys)
            uint32_t hash;              // Used by StringTable itself
        };

        typedef std::pair<slice, info> slot;

        size_t count() const                        {return _count;}
        size_t tableSize() const                    {return _size;}

        void clear() noexcept;

        slot& find(slice key) const noexcept        {return find(key, key.hash());}

        void add(slice, const info&);

        void addAt(slot&, slice key, const info&) noexcept;

        class iterator {
        public:
            operator slice () const                 {return _slot->first;}
            const slice* operator* () const         {return &_slot->first;}
            const slice* operator-> () const        {return &_slot->first;}
            info value()                            {return _slot->second;}
            iterator& operator++ ()                 {++_slot; return *this;}
            bool operator!= (const iterator &iter)  {return _slot != iter._slot;}
        private:
            iterator(const slot *s)                 :_slot(s) { }

            const slot *_slot;
            friend class StringTable;
        };

        iterator begin() const                      {return iterator(&_table[0]);}
        iterator end() const                        {return iterator(&_table[_size]);}

        void dump() const noexcept;

    private:
        void allocTable(size_t size);
        slot& find(fleece::slice key, uint32_t hash) const noexcept;
        bool _add(slice, uint32_t h, const info&) noexcept;
        void incCount()                             {if (++_count > _maxCount) grow();}
        void grow();

        static const size_t kInitialTableSize = 64;

        slot *_table;
        size_t _size;
        size_t _count;
        size_t _maxCount;
        slot _initialTable[kInitialTableSize];
    };

}
