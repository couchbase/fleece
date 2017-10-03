//
//  MutableDict.hh
//  Fleece
//
//  Created by Jens Alfke on 9/20/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "Dict.hh"
#include "MutableArray.hh"
#include <deque>
#include <unordered_map>
#include <memory>

namespace fleece {

    /** A mutable subclass of Dict. */
    class MutableDict : public Dict {
    public:

        MutableDict()
        :Dict(internal::kSpecialTag, internal::kSpecialValueMutableDict)
        { }

        /** Constructs a mutable copy of the given Dict. */
        MutableDict(const Dict*);

        const Dict* source() const                          {return _source;}
        bool isChanged() const                              {return !_map.empty();}

        // Warning: Modifying a MutableDict invalidates all Dict::iterators on it!

        template <typename T>
        void set(slice key, T value) {
            _mutableValueToSetFor(key).set(value);
        }

        void remove(slice key);
        void removeAll();

        /** Promotes an Array value to a MutableArray (in place) and returns it.
            Or if the value is already a MutableArray, just returns it. Else returns null. */
        MutableArray* makeArrayMutable(slice key)  {return (MutableArray*)makeMutable(key, kArray);}

        /** Promotes a Dict value to a MutableDict (in place) and returns it.
            Or if the value is already a MutableDict, just returns it. Else returns null. */
        MutableDict* makeDictMutable(slice key)    {return (MutableDict*)makeMutable(key, kDict);}

        class iterator {
        public:
            iterator(const MutableDict&) noexcept;

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
        };

        // Dict overrides:
        uint32_t count() const noexcept                         {return _count;}
        const Value* get(slice keyToFind) const noexcept;
        const Value* get(int numericKeyToFind) const noexcept   {abort();}
        const Value* get(key&) const noexcept                   {abort();}
        size_t get(key keys[], const Value* values[], size_t count) const noexcept {abort();}

    private:
        internal::MutableValue* _findMutableValueFor(slice keyToFind) const noexcept;
        internal::MutableValue& _makeMutableValueFor(slice key);
        internal::MutableValue& _mutableValueToSetFor(slice key);

        const Value* makeMutable(slice key, valueType ifType);
        MutableArray* kvArray();
        void markChanged()                                      {_iterable.reset();}

        const Dict* _source {nullptr};
        std::map<slice, internal::MutableValue> _map;
        std::deque<alloc_slice> _backingSlices;
        std::unique_ptr<MutableArray> _iterable;
        uint32_t _count {0};

        friend class Array::impl;
    };
}
