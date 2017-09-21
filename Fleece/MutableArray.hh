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


    class MutableArray : public Array {
    public:
        MutableArray()
        :Array(internal::kSpecialTag, internal::kSpecialValueMutableArray)
        { }

        MutableArray(uint32_t count)
        :Array(internal::kSpecialTag, internal::kSpecialValueMutableArray)
        ,_items(count)
        { }

        MutableArray(const Array*);

        uint32_t count() const                      {return (uint32_t)_items.size();}

        bool isChanged() const;

        template <typename T>
        void set(uint32_t index, T t)               {_items[index].set(t);}

        MutableArray* makeArrayMutable(uint32_t i)  {return _items[i].makeArrayMutable();}
        MutableDict* makeDictMutable(uint32_t i)    {return _items[i].makeDictMutable();}

        // Warning: Changing the size of a MutableArray invalidates all Array::iterators on it!

        template <typename T>
        void append(T t) {
            _items.emplace_back();
            _items.back().set(t);
        }
        
        void resize(uint32_t newSize);
        void insert(uint32_t where, uint32_t n);
        void remove(uint32_t where, uint32_t n);

    protected:
        const internal::MutableValue* first() const           {return &_items[0];}

    private:
        std::vector<internal::MutableValue> _items;
        bool _changed {false};

        friend class Array::impl;
    };
}
