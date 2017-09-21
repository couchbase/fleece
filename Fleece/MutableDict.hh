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

    class MutableDict : public Dict {
    public:

        MutableDict()
        :Dict(internal::kSpecialTag, internal::kSpecialValueMutableDict)
        { }

        MutableDict(uint32_t capacity)
        :Dict(internal::kSpecialTag, internal::kSpecialValueMutableDict)
        ,_map(capacity)
        { }

        MutableDict(const Dict*);

        // Warning: Modifying a MutableDict invalidates all Dict::iterators on it!

        template <typename T>
        void set(slice key, T value) {
            makeValueForKey(key).set(value);
        }

        void remove(slice key);
        void removeAll();

        void sortKeys(bool s);

        // Dict overrides:
        uint32_t count() const noexcept                         {return (uint32_t)_map.size();}
        const Value* get(slice keyToFind) const noexcept;
        const Value* get(int numericKeyToFind) const noexcept   {abort();}
        const Value* get(key&) const noexcept                   {abort();}
        size_t get(key keys[], const Value* values[], size_t count) const noexcept {abort();}

    private:
        internal::MutableValue& makeValueForKey(slice key);
        MutableArray* kvArray();

        std::unordered_map<slice, internal::MutableValue, sliceHash> _map;
        std::deque<alloc_slice> _backingSlices;
        std::unique_ptr<MutableArray> _kvArray;
        bool _sortKeys {false};

        friend class Array::impl;
    };
}
