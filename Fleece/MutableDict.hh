//
// MutableDict.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "Dict.hh"
#include "MutableValue.hh"
#include <deque>
#include <unordered_map>
#include <memory>

namespace fleece {
    class MutableArray;

    class MutableDict : public internal::MutableCollection {
    public:

        static MutableDict* asMutable(const Dict *dict) {
            return (MutableDict*)MutableCollection::asMutable(dict);
        }

        /** Constructs a mutable copy of the given Dict. */
        MutableDict(const Dict* =nullptr);

        const Dict* asDict() const                          {return (const Dict*)asValue();}

        const Dict* source() const                          {return _source;}

        uint32_t count() const noexcept                     {return _count;}
        bool empty() const                                  {return _count == 0;}
        
        const Value* get(slice keyToFind) const noexcept;

        // Warning: Modifying a MutableDict invalidates all Dict::iterators on it!

        template <typename T>
        void set(slice key, T value) {
            _mutableValueToSetFor(key).set(value);
        }

        void remove(slice key);
        void removeAll();

        /** Promotes an Array value to a MutableArray (in place) and returns it.
            Or if the value is already a MutableArray, just returns it. Else returns null. */
        MutableArray* makeArrayMutable(slice key)  {return (MutableArray*)makeMutable(key, internal::kArrayTag);}

        /** Promotes a Dict value to a MutableDict (in place) and returns it.
            Or if the value is already a MutableDict, just returns it. Else returns null. */
        MutableDict* makeDictMutable(slice key)    {return (MutableDict*)makeMutable(key, internal::kDictTag);}


        class iterator {
        public:
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
            std::map<slice, internal::MutableValue>::const_iterator _newIter, _newEnd;
            bool _sourceActive, _newActive;
            slice _sourceKey;
            uint32_t _count;
        };

    protected:
        friend class Array;

        MutableArray* kvArray();

    private:
        void markChanged();
        internal::MutableValue* _findValueFor(slice keyToFind) const noexcept;
        internal::MutableValue& _makeValueFor(slice key);
        internal::MutableValue& _mutableValueToSetFor(slice key);

        MutableCollection* makeMutable(slice key, internal::tags ifType);

        uint32_t _count {0};
        const Dict* _source {nullptr};
        std::map<slice, internal::MutableValue> _map;
        std::deque<alloc_slice> _backingSlices;
        std::unique_ptr<MutableArray> _iterable;
    };
}
