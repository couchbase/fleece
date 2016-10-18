//
//  StringTable.hh
//  Fleece
//
//  Created by Jens Alfke on 12/2/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//

#pragma once

#include "slice.hh"

namespace fleece {

    /** Internal hash table mapping strings (slices) to offsets (uint32_t). */
    class StringTable {
    public:
        StringTable(size_t capacity =0);
        ~StringTable();

        struct info {
            unsigned usedAsKey  : 1;    // Used by Encoder
            unsigned offset     :31;    // Used by Encoder
            uint32_t hash;              // only field used by StringTable itself
        };

        typedef std::pair<slice, info> slot;

        size_t count() const                        {return _count;}
        size_t tableSize() const                    {return _size;}

        void clear() noexcept;

        slot* find(slice key) const noexcept        {return find(key, key.hash());}

        void add(slice, const info&);

        void addAt(slot*, slice key, const info&) noexcept;

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

    private:
        void allocTable(size_t size);
        slot* find(fleece::slice key, uint32_t hash) const noexcept;
        bool _add(slice, uint32_t h, const info&) noexcept;
        void incCount()                             {if (++_count > _maxCount) grow();}
        void grow();

        static const size_t kInitialTableSize = 16;

        slot *_table;
        size_t _size;
        size_t _count;
        size_t _maxCount;
        slot _initialTable[kInitialTableSize];
    };

}
