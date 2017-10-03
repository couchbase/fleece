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
    :MutableDict()
    {
        _count = d->count();
        if (_usuallyFalse(d->isMutableDict())) {
            auto md = (MutableDict*)d;
            _source = md->_source;
            _map = md->_map;
            _backingSlices = md->_backingSlices;
        } else {
            _source = d;
        }
    }


    MutableValue* MutableDict::_findMutableValueFor(slice key) const noexcept {
        auto it = _map.find(key);
        if (it == _map.end())
            return nullptr;
        return const_cast<MutableValue*>(&it->second);
    }


    MutableValue& MutableDict::_makeMutableValueFor(slice key) {
        // Look in my map first:
        auto it = _map.find(key);
        if (it != _map.end())
            return it->second;
        // If not in map, add it as an empty value:
        markChanged();
        alloc_slice allocedKey(key);
        _backingSlices.push_back(allocedKey);
        return _map[allocedKey];    // allocates new pair
    }


    MutableValue& MutableDict::_mutableValueToSetFor(slice key) {
        auto &val = _makeMutableValueFor(key);
        if (!val.exists() && !(_source && _source->get(key)))
            ++_count;
        return val;
    }


    const Value* MutableDict::get(slice key) const noexcept {
        auto val = _findMutableValueFor(key);
        if (val)
            return val->exists() ? val->deref() : nullptr;
        else
            return _source ? _source->get(key) : nullptr;
    }

    
    const Value* MutableDict::makeMutable(slice key, valueType ifType) {
        auto val = get(key);
        if (_usuallyFalse(!val || val->type() != ifType))
            return nullptr;
        if (_usuallyFalse(val->asMutableArray() || val->asMutableDict()))
            return val;

        MutableValue &mval = _makeMutableValueFor(key);
        if (!mval.exists())
            mval.set(val);
        return mval.makeMutable(ifType);
    }


    void MutableDict::remove(slice key) {
        if (_source && _source->get(key)) {
            auto &val = _makeMutableValueFor(key);
            if (_usuallyFalse(!val.exists()))
                return;                             // already removed
            val.setNonexistent();
        } else {
            if (_usuallyFalse(!_map.erase(key)))    //OPT: Should remove it from _backingSlices too
                return;
        }
        --_count;
        markChanged();
    }


    void MutableDict::removeAll() {
        if (_count == 0)
            return;
        _map.clear();
        _backingSlices.clear();
        if (_source) {
            for (Dict::iterator i(_source); i; ++i)
                _makeMutableValueFor(i.keyString());    // override source with empty values
        }
        _count = 0;
        markChanged();
    }


    MutableArray* MutableDict::kvArray() {
        if (!_iterable) {
            _iterable.reset(new MutableArray(2*count()));
            uint32_t n = 0;
            for (iterator i(*this); i; ++i) {
                _iterable->set(n++, i.keyString());
                _iterable->set(n++, i.value());
            }
            assert(n == 2*_count);
        }
        return _iterable.get();
    }


#pragma mark - ITERATOR:


    MutableDict::iterator::iterator(const MutableDict &dict) noexcept
    :_sourceIter(dict._source)
    ,_newIter(dict._map.begin())
    ,_newEnd(dict._map.end())
    {
        getSource();
        getNew();
        ++(*this);
    }

    void MutableDict::iterator::getSource() {
        _sourceActive = (bool)_sourceIter;
        if (_usuallyTrue(_sourceActive))
            _sourceKey = _sourceIter.keyString();
    }

    void MutableDict::iterator::getNew() {
        _newActive = _newIter != _newEnd;
    }


    MutableDict::iterator& MutableDict::iterator::operator++() {
        // Since _source and _map are both sorted, this is basically just an array merge.
        // Special cases: both items might be equal, or the item from _map might be a tombstone.
        while (_usuallyTrue(_sourceActive || _newActive)) {
            if (!_newActive || (_sourceActive && _sourceKey < _newIter->first)) {
                // Key from _source is lower, so add its pair:
                _key = _sourceKey;
                _value = _sourceIter.value();
                ++_sourceIter;
                getSource();
                return *this;
            } else {
                bool exists = (_newIter->second.exists());
                if (_usuallyTrue(exists)) {
                    // Key from _map is lower or equal, and its value exists, so add its pair:
                    _key = _newIter->first;
                    _value = _newIter->second.deref();
                }
                if (_sourceActive && _sourceKey == _newIter->first) {
                    ++_sourceIter;
                    getSource();
                }
                ++_newIter;
                getNew();
                if (_usuallyTrue(exists))
                    return *this;
                // If the value doesn't exist, go around again to get one that does...
            }
        }
        // Value didn't exist, but no more values:
        _value = nullptr;
        return *this;
    }


}
