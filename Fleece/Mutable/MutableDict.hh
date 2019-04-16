//
// MutableDict.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
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
