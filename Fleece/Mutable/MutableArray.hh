//
//  MutableArray.hh
//  Fleece
//
//  Created by Jens Alfke on 5/7/18.
//  Copyright 2018-Present Couchbase, Inc.
//
//  Use of this software is governed by the Business Source License included
//  in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
//  in that file, in accordance with the Business Source License, use of this
//  software will be governed by the Apache License, Version 2.0, included in
//  the file licenses/APL2.txt.
//

#pragma once
#include "Array.hh"
#include "HeapArray.hh"

namespace fleece { namespace impl {
    class MutableDict;


    class MutableArray : public Array {
    public:

        /** Creates a new array of size `initialCount` filled with null Values. */
        static Retained<MutableArray> newArray(uint32_t initialCount =0) {
            return (new internal::HeapArray(initialCount))->asMutableArray();
        }

        /** Creates a copy of `a`, or an empty array if `a` is null.
            If `deepCopy` is true, nested mutable collections will be recursively copied too. */
        static Retained<MutableArray> newArray(const Array *a, CopyFlags flags =kDefaultCopy) {
            auto ha = retained(new internal::HeapArray(a));
            if (flags)
                ha->copyChildren(flags);
            return ha->asMutableArray();
        }

        Retained<MutableArray> copy(CopyFlags f = kDefaultCopy)    {return newArray(this, f);}

        const Array* source() const                 {return heapArray()->_source;}
        bool isChanged() const                      {return heapArray()->isChanged();}
        void setChanged(bool changed)               {heapArray()->setChanged(changed);}

        ValueSlot& setting(uint32_t index)          {return heapArray()->setting(index);}
        ValueSlot& inserting(uint32_t index)        {return heapArray()->inserting(index);}
        ValueSlot& appending()                      {return heapArray()->appending();}

        template <typename T>
        void set(uint32_t index, T t)               {heapArray()->set(index, t);}

        // Warning: Changing the size of a MutableArray invalidates pointers to items that are
        // small scalar values, and also invalidates iterators.

        /** Appends a new Value. */
        template <typename T>  void append(const T &t)     {heapArray()->append(t);}

        void resize(uint32_t newSize)               {heapArray()->resize(newSize);}
        void insert(uint32_t where, uint32_t n)     {heapArray()->insert(where, n);}
        void remove(uint32_t where, uint32_t n)     {heapArray()->remove(where, n);}

        /** Promotes an Array item to a MutableArray (in place) and returns it.
            Or if the item is already a MutableArray, just returns it. Else returns null. */
        MutableArray* getMutableArray(uint32_t i)   {return heapArray()->getMutableArray(i);}

        /** Promotes a Dict item to a MutableDict (in place) and returns it.
            Or if the item is already a HeapDict, just returns it. Else returns null. */
        MutableDict* getMutableDict(uint32_t i)     {return heapArray()->getMutableDict(i);}

        using iterator = internal::HeapArray::iterator;
    };
    
} }
