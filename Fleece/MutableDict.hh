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

        MutableDict(uint32_t initialCapacity)
        :Dict(internal::kSpecialTag, internal::kSpecialValueMutableDict)
        ,_map(initialCapacity)
        { }

        /** Constructs a mutable copy of the given Dict. */
        MutableDict(const Dict*);

        bool isChanged() const;

        // Warning: Modifying a MutableDict invalidates all Dict::iterators on it!

        template <typename T>
        void set(slice key, T value) {
            makeValueForKey(key).set(value);
        }

        void remove(slice key);
        void removeAll();

        /** Sets whether iterators will visit keys in sorted order (as in a regular Dict.)
            The default is false. */
        void sortKeys(bool s);

        /** Promotes an Array value to a MutableArray (in place) and returns it.
            Or if the value is already a MutableArray, just returns it. Else returns null. */
        MutableArray* makeArrayMutable(slice key);

        /** Promotes a Dict value to a MutableDict (in place) and returns it.
            Or if the value is already a MutableDict, just returns it. Else returns null. */
        MutableDict* makeDictMutable(slice key);

        // Dict overrides:
        uint32_t count() const noexcept                         {return (uint32_t)_map.size();}
        const Value* get(slice keyToFind) const noexcept;
        const Value* get(int numericKeyToFind) const noexcept   {abort();}
        const Value* get(key&) const noexcept                   {abort();}
        size_t get(key keys[], const Value* values[], size_t count) const noexcept {abort();}

    private:
        internal::MutableValue* _get(slice keyToFind) const noexcept;
        internal::MutableValue& makeValueForKey(slice key);
        MutableArray* kvArray();
        void markChanged();

        std::unordered_map<slice, internal::MutableValue, sliceHash> _map;
        std::deque<alloc_slice> _backingSlices;
        std::unique_ptr<MutableArray> _kvArray;
        bool _changed {false};
        bool _sortKeys {false};

        friend class Array::impl;
    };
}
