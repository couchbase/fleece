//
// HeapArray.hh
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "Array.hh"
#include "ValueSlot.hh"
#include <vector>
#include "betterassert.hh"

namespace fleece { namespace impl {
    class MutableArray;
    class MutableDict;
} }

namespace fleece { namespace impl { namespace internal {

    class HeapArray : public HeapCollection {
    public:
        HeapArray()
        :HeapCollection(kArrayTag)
        { }

        HeapArray(uint32_t initialCount)
        :HeapCollection(kArrayTag)
        ,_items(initialCount)
        { }

        HeapArray(const Array*);

        static MutableArray* asMutableArray(HeapArray *a)   {return (MutableArray*)asValue(a);}
        MutableArray* asMutableArray() const        {return (MutableArray*)asValue();}

        uint32_t count() const                      {return (uint32_t)_items.size();}
        bool empty() const                          {return _items.empty();}

        const Array* source() const                 {return _source;}

        const Value* get(uint32_t index);

        ValueSlot& setting(uint32_t index);
        ValueSlot& appending();
        ValueSlot& inserting(uint32_t index)        {insert(index, 1); return setting(index);}


        template <typename T>
        void set(uint32_t index, T t)               {setting(index).set(t);}

        // Warning: Changing the size of a MutableArray invalidates pointers to items that are
        // small scalar values, and also invalidates iterators.

        /** Appends a new Value. */
        template <typename T>
        void append(const T &t)                     {appending().set(t);}


        void resize(uint32_t newSize);              ///< Appends nulls, or removes items from end
        void insert(uint32_t where, uint32_t n);    ///< Inserts `n` nulls at index `where`
        void remove(uint32_t where, uint32_t n);    ///< Removes items starting at index `where`

        /** Promotes an Array item to a MutableArray (in place) and returns it.
            Or if the item is already a MutableArray, just returns it. Else returns null. */
        MutableArray* getMutableArray(uint32_t i);

        /** Promotes a Dict item to a MutableDict (in place) and returns it.
            Or if the item is already a MutableDict, just returns it. Else returns null. */
        MutableDict* getMutableDict(uint32_t i);

        void disconnectFromSource();
        void copyChildren(CopyFlags flags);

        class iterator {
        public:
            iterator(const HeapArray* FL_NONNULL) noexcept;
            iterator(const MutableArray* FL_NONNULL) noexcept;

            const Value* value() const noexcept             {return _value;}

            /** Returns false when the iterator reaches the end. */
            explicit operator bool() const noexcept          {return _value != nullptr;}

            /** Steps to the next item. (Throws if there are no more items.) */
            iterator& operator ++();

        private:
            const Value* _value;
            std::vector<ValueSlot>::const_iterator _iter, _iterEnd;
            Array::iterator _sourceIter;
            uint32_t _index {0};
        };


    protected:
        friend class impl::Array;
        friend class impl::MutableArray;

        ~HeapArray() =default;
        const ValueSlot* first();          // Called by Array::impl

    private:
        void populate(unsigned fromIndex);
        HeapCollection* getMutable(uint32_t index, tags ifType);

        // _items stores each array item as a ValueSlot. If an item's type is 'undefined',
        // that means the item is unchanged and its value can be found at the same index in _source.
        std::vector<ValueSlot> _items;

        // The original Array that this is a mutable copy of.
        RetainedConst<Array> _source;
    };
    
} } }
