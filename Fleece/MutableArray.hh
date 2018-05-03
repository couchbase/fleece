//
// MutableArray.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "Array.hh"
#include "MutableValue.hh"
#include <vector>

namespace fleece {
    class MutableDict;


    /** A mutable subclass of Array. */
    class MutableArray : public internal::MutableCollection {
    public:
        MutableArray()
        :MutableCollection(internal::kArrayTag)
        { }

        MutableArray(uint32_t initialCount)
        :MutableCollection(internal::kArrayTag)
        ,_items(initialCount)
        { }

        /** Constructs a mutable copy of the given Array. */
        MutableArray(const Array* NONNULL);

        uint32_t count() const                      {return (uint32_t)_items.size();}
        bool empty() const                          {return _items.empty();}

        const Array* source() const                 {return _source;}

        const Value* get(uint32_t index);

        template <typename T>
        void set(uint32_t index, T t)               {_items[index].set(t); _changed = true;}

        /** Promotes an Array item to a MutableArray (in place) and returns it.
         Or if the item is already a MutableArray, just returns it. Else returns null. */
        MutableArray* makeArrayMutable(uint32_t i)  {return (MutableArray*)_items[i].makeMutable(internal::kArrayTag);}

        /** Promotes a Dict item to a MutableDict (in place) and returns it.
         Or if the item is already a MutableDict, just returns it. Else returns null. */
        MutableDict* makeDictMutable(uint32_t i)    {return (MutableDict*)_items[i].makeMutable(internal::kDictTag);}

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


        class iterator {
        public:
            iterator(const MutableArray* NONNULL) noexcept;

            const Value* value() const noexcept             {return _value;}

            /** Returns false when the iterator reaches the end. */
            explicit operator bool() const noexcept          {return _value != nullptr;}

            /** Steps to the next item. (Throws if there are no more items.) */
            iterator& operator ++();

        private:
            const Value* _value;
            std::vector<internal::MutableValue>::const_iterator _iter, _iterEnd;
            Array::iterator _sourceIter;
        };


    protected:
        const internal::MutableValue* first() const           {return &_items.front();}

    private:
        void populate(unsigned fromIndex);

        std::vector<internal::MutableValue> _items; // Undefined items shadow _source
        const Array* _source {nullptr};
    };
}
