//
// HeapDict.cc
//
// Copyright © 2018 Couchbase. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "HeapDict.hh"
#include "HeapArray.hh"
#include "ValueSlot.hh"
#include "MutableDict.hh"
#include "Encoder.hh"
#include "SharedKeys.hh"
#include "betterassert.hh"

namespace fleece { namespace impl { namespace internal {

    HeapDict::HeapDict(const Dict *d)
    :HeapCollection(kDictTag)
    {
        if (d) {
            _count = d->count();
            if (d->isMutable()) {
                auto hd = d->asMutable()->heapDict();
                _source = hd->_source;
                _map = hd->_map;
                _backingSlices = hd->_backingSlices;
            } else {
                _source = d;
            }
            if (_source)
                _sharedKeys = _source->sharedKeys();
        }
    }


    void HeapDict::markChanged() {
        setChanged(true);
        _iterable = nullptr;
    }


    key_t HeapDict::encodeKey(slice key) const noexcept {
        int intKey;
        if (_sharedKeys && _sharedKeys->encode(key, intKey))
            return intKey;
        return key;
    }


    ValueSlot* HeapDict::_findValueFor(slice key) const noexcept {
        if (_map.empty())
            return nullptr;
        key_t encoded = encodeKey(key);
        auto slot = _findValueFor(encoded);
        if (!slot && encoded.shared()) {
            // This string might have become a shared key after I added it, in which case the
            // above lookup for the encoded (int) version would fail. Try again with the string
            // version:
            slot = _findValueFor(key_t(key));
        }
        return slot;
    }


    ValueSlot* HeapDict::_findValueFor(key_t key) const noexcept {
        auto it = _map.find(key);
        if (it == _map.end())
            return nullptr;
        return const_cast<ValueSlot*>(&it->second);
    }


    key_t HeapDict::_allocateKey(key_t key) {
        if (key.shared())
            return key;
        alloc_slice allocedKey(key.asString());
        _backingSlices.push_back(allocedKey);
        return key_t(allocedKey);
    }


    ValueSlot& HeapDict::_makeValueFor(key_t key) {
        // Look in my map first:
        auto it = _map.find(key);
        if (it != _map.end())
            return it->second;
        // If not in map, add it as an empty value:
        return _map[key_t(_allocateKey(key))];                // creates a new value
    }


    // this is the innards of the set() method
    ValueSlot& HeapDict::setting(slice stringKey) {
        key_t key;
        ValueSlot *slotp = _findValueFor(stringKey);
        if (slotp) {
            key = stringKey;
        } else {
            key = encodeKey(stringKey);
            slotp = &_makeValueFor(key);
        }
        if (slotp->empty() && !(_source && _source->get(key)))
            ++_count;
        markChanged();
        return *slotp;
    }


    const Value* HeapDict::get(slice key) const noexcept {
        ValueSlot* val = _findValueFor(key);
        if (val)
            return val->asValue();
        else
            return _source ? _source->get(key) : nullptr;
    }


    const Value* HeapDict::get(int key) const noexcept {
        auto it = _map.find(key);
        if (it != _map.end())
            return it->second.asValue();
        else
            return _source ? _source->get(key) : nullptr;
    }


    const Value* HeapDict::get(Dict::key &key) const noexcept {
        ValueSlot* val = _findValueFor(key.string());
        if (val)
            return val->asValue();
        else
            return _source ? _source->get(key) : nullptr;
    }


    const Value* HeapDict::get(const key_t &key) const noexcept {
        auto it = _map.find(key);
        if (it != _map.end())
            return it->second.asValue();
        else
            return _source ? _source->get(key) : nullptr;
    }


    HeapCollection* HeapDict::getMutable(slice stringKey, tags ifType) {
        key_t key = encodeKey(stringKey);
        Retained<HeapCollection> result;
        ValueSlot* mval = _findValueFor(key);
        if (mval) {
            result = mval->makeMutable(ifType);
        } else if (_source) {
            result = HeapCollection::mutableCopy(_source->get(key), ifType);
            if (result)
                _map.emplace(_allocateKey(key), result.get());
        }
        if (result)
            markChanged();
        return result;
    }


    void HeapDict::remove(slice stringKey) {
        key_t key = encodeKey(stringKey);
        if (_source && _source->get(key)) {
            auto it = _map.find(key);
            if (it != _map.end()) {
                if (_usuallyFalse(!it->second))
                    return;                             // already removed
                it->second = ValueSlot();
            } else {
                _makeValueFor(key);
            }
        } else {
            if (_usuallyFalse(!_map.erase(key)))        //OPT: key remains in _backingSlices
                return;
        }
        --_count;
        markChanged();
    }


    void HeapDict::removeAll() {
        if (_count == 0)
            return;
        _map.clear();
        _backingSlices.clear();
        if (_source) {
            for (Dict::iterator i(_source); i; ++i)
                _makeValueFor(i.keyt());    // override source with empty values
        }
        _count = 0;
        markChanged();
    }


    HeapArray* HeapDict::kvArray() {
        if (!_iterable) {
            _iterable = new HeapArray(2*count());
            uint32_t n = 0;
            for (iterator i(this); i; ++i) {
                _iterable->set(n++, i.keyString());
                _iterable->set(n++, i.value());
            }
            assert(n == 2*_count);
        }
        return _iterable.get();
    }


    bool HeapDict::tooManyAncestors() const {
        auto grampaw = _source->getParent();
        return grampaw && grampaw->getParent();
    }


    void HeapDict::writeTo(Encoder &enc) {
        if (enc.valueIsInBase(_source) && _map.size() + 1 < count() && !tooManyAncestors()) {
            // Write just the changed keys, with _source as parent:
            enc.beginDictionary(_source, _map.size());
            for (auto &i : _map) {
                enc.writeKey(i.first);
                enc.writeValue(i.second.asValueOrUndefined());
            }
            enc.endDictionary();
        } else {
            iterator iter(this);
            enc.beginDictionary(iter.count());
            for (; iter; ++iter) {
                enc.writeKey(iter.keyString());
                enc.writeValue(iter.value());
            }
            enc.endDictionary();
        }
    }


    void HeapDict::disconnectFromSource() {
        if (!_source)
            return;
        for (Dict::iterator i(_source); i; ++i) {
            slice key = i.keyString();
            if (_map.find(key) == _map.end())
                set(key, i.value());
        }
        _source = nullptr;
    }


    void HeapDict::copyChildren(CopyFlags flags) {
        if (flags & kCopyImmutables)
            disconnectFromSource();
        for (auto &entry : _map)
            entry.second.copyValue(flags);
    }


#pragma mark - ITERATOR:


    HeapDict::iterator::iterator(const HeapDict *dict) noexcept
    :_sourceIter(dict->_source)
    ,_newIter(dict->_map.begin())
    ,_newEnd(dict->_map.end())
    ,_count(dict->count() + 1)
    ,_sharedKeys(dict->sharedKeys())
    {
        getSource();
        getNew();
        ++(*this);
    }

    HeapDict::iterator::iterator(const MutableDict *dict) noexcept
    :iterator((HeapDict*)HeapCollection::asHeapValue(dict))
    { }

    void HeapDict::iterator::getSource() {
        _sourceActive = (bool)_sourceIter;
        if (_usuallyTrue(_sourceActive))
            _sourceKey = _sourceIter.key();
    }

    void HeapDict::iterator::getNew() {
        _newActive = (_newIter != _newEnd);
    }

    void HeapDict::iterator::decodeKey(key_t key) {
        if (key.shared())
            _key = _sharedKeys->decode(key.asInt());
        else
            _key = key.asString();
    }


    HeapDict::iterator& HeapDict::iterator::operator++() {
        // Since _source and _map are both sorted, this is basically just an array merge.
        // Special cases: both items might be equal, or the item from _map might be a tombstone.
        --_count;
        while (_usuallyTrue(_sourceActive || _newActive)) {
            if (!_newActive || (_sourceActive && _sourceKey < _newIter->first)) {
                // Key from _source is lower, so add its pair:
                decodeKey(_sourceKey);
                _value = _sourceIter.value();
                ++_sourceIter;
                getSource();
                return *this;
            } else {
                bool exists = !!(_newIter->second);
                if (_usuallyTrue(exists)) {
                    // Key from _map is lower or equal, and its value exists, so add its pair:
                    decodeKey(_newIter->first);
                    _value = _newIter->second.asValue();
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

} } }
