//
//  MutableArray.hh
//  Fleece
//
//  Created by Jens Alfke on 9/19/17.
//Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "Array.hh"
#include "MutableValue.hh"

namespace fleece {
    class MutableDict;


    /** A mutable subclass of Array. */
    class MutableArray : public Array {
    public:
        MutableArray()
        :Array(internal::kSpecialTag, internal::kSpecialValueMutableArray)
        { }

        MutableArray(uint32_t initialCount)
        :Array(internal::kSpecialTag, internal::kSpecialValueMutableArray)
        ,_items(initialCount)
        { }

        /** Constructs a mutable copy of the given Array. */
        MutableArray(const Array*);

        uint32_t count() const                      {return (uint32_t)_items.size();}

        const Array* source() const                 {return _source;}
        bool isChanged() const;

        template <typename T>
        void set(uint32_t index, T t)               {_items[index].set(t);}

        /** Promotes an Array item to a MutableArray (in place) and returns it.
            Or if the item is already a MutableArray, just returns it. Else returns null. */
        MutableArray* makeArrayMutable(uint32_t i)  {return (MutableArray*)_items[i].makeMutable(kArray);}

        /** Promotes a Dict item to a MutableDict (in place) and returns it.
            Or if the item is already a MutableDict, just returns it. Else returns null. */
        MutableDict* makeDictMutable(uint32_t i)    {return (MutableDict*)_items[i].makeMutable(kDict);}

        // Warning: Changing the size of a MutableArray invalidates all Array::iterators on it!

        /** Appends a new Value. */
        template <typename T>
        void append(T t) {
            _items.emplace_back();
            _items.back().set(t);
        }
        
        void resize(uint32_t newSize);              ///< Appends nulls, or removes items from end
        void insert(uint32_t where, uint32_t n);    ///< Inserts `n` nulls at index `where`
        void remove(uint32_t where, uint32_t n);    ///< Removes items starting at index `where`
        void removeAll();

    protected:
        const internal::MutableValue* first() const           {return &_items.front();}

    private:
        std::vector<internal::MutableValue> _items;
        const Array* _source {nullptr};
        bool _changed {false};

        friend class Array::impl;
    };
}
