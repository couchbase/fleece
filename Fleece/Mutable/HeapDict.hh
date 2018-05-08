//
// HeapDict.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "Dict.hh"
#include "ValueSlot.hh"
#include <deque>
#include <unordered_map>
#include <memory>

namespace fleece {
    class Encoder;
    class SharedKeys;
}

namespace fleece { namespace internal {
    class HeapArray;

    class HeapDict : public HeapCollection {
    public:
        HeapDict(const Dict* =nullptr);

        static MutableDict* asMutableDict(HeapDict *a)   {return (MutableDict*)asValue(a);}
        MutableDict* asMutableDict() const        {return (MutableDict*)asValue();}

        const Dict* source() const                          {return _source;}

        uint32_t count() const                              {return _count;}
        bool empty() const                                  {return _count == 0;}
        
        const Value* get(slice keyToFind) const noexcept;

        // Warning: Modifying a HeapDict invalidates all Dict::iterators on it!

        template <typename T>
        void set(slice key, T value)                        {_mutableValueToSetFor(key).set(value);}

        void remove(slice key);
        void removeAll();

        /** Promotes an Array value to a MutableArray (in place) and returns it.
            Or if the value is already a MutableArray, just returns it. Else returns null. */
        MutableArray* getMutableArray(slice key)  {return (MutableArray*)asValue(getMutable(key, kArrayTag));}

        /** Promotes a Dict value to a HeapDict (in place) and returns it.
            Or if the value is already a HeapDict, just returns it. Else returns null. */
        MutableDict* getMutableDict(slice key)    {return (MutableDict*)asValue(getMutable(key, kDictTag));}


        class iterator {
        public:
            iterator(const HeapDict* NONNULL) noexcept;
            iterator(const MutableDict* NONNULL) noexcept;

            uint32_t count() const noexcept                 {return _count;}
            
            slice keyString() const noexcept                {return _key;}
            const Value* value() const noexcept             {return _value;}

            /** Returns false when the iterator reaches the end. */
            explicit operator bool() const noexcept          {return _value != nullptr;}

            /** Steps to the next item. (Throws if there are no more items.) */
            iterator& operator ++();

        private:
            void getSource();
            void getNew();

            slice _key;
            const Value* _value;
            Dict::iterator _sourceIter;
            std::map<slice, ValueSlot>::const_iterator _newIter, _newEnd;
            bool _sourceActive, _newActive;
            slice _sourceKey;
            uint32_t _count;
        };

        void writeTo(Encoder&, const SharedKeys*);

    protected:
        friend class fleece::Array;
        friend class fleece::MutableDict;

        ~HeapDict() =default;
        HeapArray* kvArray();

    private:
        void markChanged();
        ValueSlot* _findValueFor(slice keyToFind) const noexcept;
        ValueSlot& _makeValueFor(slice key);
        ValueSlot& _mutableValueToSetFor(slice key);
        HeapCollection* getMutable(slice key, tags ifType);
        bool tooManyAncestors() const;

        uint32_t _count {0};
        const Dict* _source {nullptr};
        std::map<slice, ValueSlot> _map;
        std::deque<alloc_slice> _backingSlices;
        Retained<HeapArray> _iterable;
    };
} }
