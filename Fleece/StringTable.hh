//
//  stringTable.hh
//  Fleece
//
//  Created by Jens Alfke on 12/2/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//

#ifndef stringTable_hh
#define stringTable_hh

#include "slice.hh"

namespace fleece {

    /** Internal hash table mapping strings (slices) to offsets (uint32_t). */
    class StringTable {
    public:
        StringTable(size_t capacity =0);
        ~StringTable();

        struct info {
            uint32_t offset;
            uint32_t hash;
            bool usedAsKey;
        };

        typedef std::pair<slice, info> slot;

        size_t count()                              {return _count;}
        size_t tableSize()                          {return _size;}

        void clear();

        slot* find(slice key)                       {return find(key, hash(key));}

        void add(slice, const info&);

        void addAt(slot*, slice, const info&);

        static uint32_t hash(slice s) {  // djb2 hash function
            uint32_t hash = 5381;
            for (size_t i = 0; i < s.size; i++)
                hash = (hash<<5) + hash + s[i];
            return hash;
        }

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
        slot* find(fleece::slice key, uint32_t hash);
        bool _add(slice, uint32_t h, const info&);
        void incCount()                             {if (++_count > _maxCount) grow();}
        void grow();

        slot *_table;
        size_t _size;
        size_t _count;
        size_t _maxCount;
    };

}

#endif /* stringTable_hh */
