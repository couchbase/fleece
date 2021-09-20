//
// MutableDict.hh
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
#include "Dict.hh"
#include "HeapDict.hh"

namespace fleece { namespace impl {
    class MutableArray;


    class MutableDict : public Dict {
    public:

        static Retained<MutableDict> newDict(const Dict *d =nullptr, CopyFlags flags =kDefaultCopy) {
            auto hd = retained(new internal::HeapDict(d));
            if (flags)
                hd->copyChildren(flags);
            return hd->asMutableDict();
        }

        Retained<MutableDict> copy(CopyFlags f =kDefaultCopy) {return newDict(this, f);}

        const Dict* source() const                          {return heapDict()->_source;}
        bool isChanged() const                              {return heapDict()->isChanged();}
        void setChanged(bool changed)                       {heapDict()->setChanged(changed);}

        const Value* get(slice keyToFind) const noexcept    {return heapDict()->get(keyToFind);}

        // Warning: Modifying a MutableDict invalidates all Dict::iterators on it!

        ValueSlot& setting(slice key)                       {return heapDict()->setting(key);}

        template <typename T>
        void set(slice key, T value)                        {heapDict()->set(key, value);}

        void remove(slice key)                              {heapDict()->remove(key);}
        void removeAll()                                    {heapDict()->removeAll();}

        /** Promotes an Array value to a MutableArray (in place) and returns it.
            Or if the value is already a MutableArray, just returns it. Else returns null. */
        MutableArray* getMutableArray(slice key)        {return heapDict()->getMutableArray(key);}

        /** Promotes a Dict value to a MutableDict (in place) and returns it.
            Or if the value is already a MutableDict, just returns it. Else returns null. */
        MutableDict* getMutableDict(slice key)          {return heapDict()->getMutableDict(key);}

        using iterator = internal::HeapDict::iterator;
    };
    
} }
