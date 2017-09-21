//
//  MutableDict.cc
//  Fleece
//
//  Created by Jens Alfke on 9/20/17.
//Copyright © 2017 Couchbase. All rights reserved.
//

#include "MutableDict.hh"

namespace fleece {
    using namespace internal;


    MutableDict::MutableDict(const Dict *d)
    :MutableDict(d->count())
    {
        for (Dict::iterator src(d); src; ++src)
            set(src.keyString(), src.value());
    }


    const Value* MutableDict::get(slice key) const noexcept {
        auto it = _map.find(key);
        if (it == _map.end())
            return nullptr;
        return it->second.deref();
    }

    
    bool MutableDict::isChanged() const {
        if (_changed)
            return true;
        for (auto &item : _map)
            if (item.second.isChanged())
                return true;
        return false;
    }


    MutableValue& MutableDict::makeValueForKey(slice key) {
        auto it = _map.find(key);
        if (it != _map.end())
            return it->second;
        markChanged();
        alloc_slice allocedKey(key);
        _backingSlices.push_back(allocedKey);
        return _map[allocedKey];    // allocates new pair
    }


    void MutableDict::remove(slice key) {
        _map.erase(key);        //FIX: Should remove it from _backingSlices too
        markChanged();
    }


    void MutableDict::removeAll() {
        _map.clear();
        _backingSlices.clear();
        markChanged();
    }


    void MutableDict::sortKeys(bool s) {
        if (s != _sortKeys) {
            _sortKeys = s;
            _kvArray.reset();
        }
    }


    void MutableDict::markChanged() {
        _kvArray.reset();
        _changed = true;
    }


    MutableArray* MutableDict::kvArray() {
        if (!_kvArray) {
            _kvArray.reset(new MutableArray(2*count()));
            uint32_t n = 0;
            if (_usuallyFalse(_sortKeys)) {
                using MapEntry = const std::pair<const slice,MutableValue>*;
                std::vector<MapEntry> entries;
                for (auto &entry : _map)
                    entries.push_back(&entry);
                std::sort(entries.begin(), entries.end(), [](MapEntry a, MapEntry b) {
                    return a->first < b->first;
                });
                for (auto entry : entries) {
                    _kvArray->set(n++, entry->first);
                    _kvArray->set(n++, entry->second.deref());
                }
            } else {
                for (auto &entry : _map) {
                    _kvArray->set(n++, entry.first);
                    _kvArray->set(n++, entry.second.deref());
                }
            }
        }
        return _kvArray.get();
    }

}
