//
//  MDict.hh
//  Fleece
//
//  Created by Jens Alfke on 5/29/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "MCollection.hh"
#include <unordered_map>
#include <vector>

namespace fleeceapi {

    /** A mutable dictionary of MValues. */
    template <class Native>
    class MDict : public MCollection<Native> {
    public:
        using MValue = MValue<Native>;
        using MCollection = MCollection<Native>;
        using MapType = std::unordered_map<slice, MValue, fleece::sliceHash>;

        MDict() { }

        void initInSlot(MValue *mv, MCollection *parent) {
            MCollection::initInSlot(mv, parent);
            _dict = mv->value().asDict();
            _count = _dict.count();
            _map.clear();
        }

        void init(const MDict &d) {
            _dict = d._dict;
            _count = d._count;
            _map = d._map;
        }

        size_t count() const {
            return _count;
        }

        bool contains(slice key) const {
            auto i = _map.find(key);
            if (i != _map.end())
                return !i->second.isEmpty();
            else
                return _dict.get(key, MCollection::sharedKeys()) != nullptr;
        }

        const MValue* get(slice key) const {
            auto i = _map.find(key);
            if (i == _map.end()) {
                auto value = _dict.get(key, MCollection::sharedKeys());
                if (!value)
                    return nullptr;
                i = const_cast<MDict*>(this)->_setInMap(key, value);
            }
            return &i->second;
        }

        void set(slice key, const MValue &val) {
            auto i = _map.find(key);
            if (i != _map.end()) {
                if (val.isEmpty() && i->second.isEmpty())
                    return;
                MCollection::mutate();
                _count += !val.isEmpty() - !i->second.isEmpty();
                i->second = val;
            } else {
                if (_dict.get(key, MCollection::sharedKeys())) {
                    if (val.isEmpty())
                        --_count;
                } else {
                    if (val.isEmpty())
                        return;
                    else
                        ++_count;
                }
                MCollection::mutate();
                _setInMap(key, val);
            }
        }

        typename MDict::MapType::iterator _setInMap(slice key, const MValue &val) {
            _newKeys.emplace_back(key);
            key = _newKeys.back();
            return _map.emplace(key, val).first;
        }

        void remove(slice key) {
            set(key, MValue());
        }

        void clear() {
            if (_count == 0)
                return;
            MCollection::mutate();
            _map.clear();
            for (Dict::iterator i(_dict, MCollection::sharedKeys()); i; ++i)
                _map.emplace(i.keyString(), MValue::empty);
            _count = 0;
        }

        template <class FN>                 // FN should be callable with (slice, const MValue&)
        void enumerate(FN callback) const {
            for (auto &item : _map) {
                if (!item.second.isEmpty())
                    callback(item.first, item.second);
            }
            for (Dict::iterator i(_dict, MCollection::sharedKeys()); i; ++i) {
                slice key = i.keyString();
                if (_map.find(key) == _map.end())
                    callback(key, MValue(i.value()));
            }
        }

        void encodeTo(Encoder &enc) const {
            if (!MCollection::isMutated()) {
                enc << _dict;
            } else {
                enc.beginDict(count());
                enumerate([&](slice key, const MValue &mv) mutable {
                    enc.writeKey(key);
                    mv.encodeTo(enc);
                });
                enc.endDict();
            }
        }

    private:
        Dict                     _dict;     // Base Fleece dict (if any)
        uint32_t                 _count {0};// Current count
        MapType                  _map;      // Maps changed keys --> MValues
        std::vector<alloc_slice> _newKeys;  // storage for new key slices for _map
    };

}
