//
// MDict.hh
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
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

#pragma once
#include "MCollection.hh"
#include <unordered_map>
#include <vector>

namespace fleeceapi {

    template <class Native>
    class MDictIterator;

    /** A mutable dictionary of MValues. */
    template <class Native>
    class MDict : public MCollection<Native> {
    public:
        using MValue = MValue<Native>;
        using MCollection = MCollection<Native>;
        using MapType = std::unordered_map<slice, MValue, fleece::sliceHash>;

        /** Constructs an empty MDict not connected to any existing Fleece Dict. */
        MDict() :MCollection() { }

        /** Constructs an MDict that shadows a Dict stored in `mv` and contained in `parent`.
            This is what you'd call from MValue::toNative. */
        MDict(MValue *mv, MCollection *parent) {
            initInSlot(mv, parent);
        }

        /** Initializes a brand-new MDict created with the empty constructor, as though it had
            been created with the existing-Dict constructor. Useful in situations where you can't
            pass parameters to the constructor (i.e. when embedding an MDict in an Objective-C++
            object.) */
        void initInSlot(MValue *mv, MCollection *parent, bool isMutable) {
            MCollection::initInSlot(mv, parent, isMutable);
            assert(!_dict);
            _dict = mv->value().asDict();
            _count = _dict.count();
            _map.reserve(5);
        }

        void initInSlot(MValue *mv, MCollection *parent) {
            initInSlot(mv, parent, parent->mutableChildren());
        }

        /** Copies the MDict d into the receiver. */
        void initAsCopyOf(const MDict &d, bool isMutable) {
            MCollection::initAsCopyOf(d, isMutable);
            _dict = d._dict;
            _map = d._map;
            _count = d._count;
        }

        /** Returns the number of items in the dictionary. */
        size_t count() const {
            return _count;
        }

        /** Returns true if the dictionary contains the given key, but doesn't return the value. */
        bool contains(slice key) const {
            auto i = _map.find(key);
            if (i != _map.end())
                return !i->second.isEmpty();
            else
                return _dict.get(key, sharedKeys()) != nullptr;
        }

        /** Returns the value for the given key, or an empty MValue if it's not found. */
        const MValue& get(slice key) const {
            auto i = _map.find(key);
            if (i == _map.end()) {
                auto value = _dict.get(key, sharedKeys());
                if (!value)
                    return MValue::empty;
                // Add an entry to the map, so that the caller can associate a Native object
                // with the value, and to speed up the next lookup
                i = const_cast<MDict*>(this)->_setInMap(key, value);
            }
            return i->second;
        }

        /** Stores a value for a key. */
        bool set(slice key, const MValue &val) {
            if (_usuallyFalse(!MCollection::isMutable()))
                return false;
            auto i = _map.find(key);
            if (i != _map.end()) {
                // Found in _map; update value:
                if (_usuallyFalse(val.isEmpty() && i->second.isEmpty()))
                    return true;    // no-op
                MCollection::mutate();
                _count += !val.isEmpty() - !i->second.isEmpty();
                i->second = val;
            } else {
                // Not found; check _dict:
                if (_dict.get(key, sharedKeys())) {
                    if (val.isEmpty())
                        --_count;
                } else {
                    if (val.isEmpty())
                        return true;    // no-op
                    else
                        ++_count;
                }
                MCollection::mutate();
                _setInMap(key, val);
            }
            return true;
        }

        /** Removes the value, if any, for a key. */
        bool remove(slice key) {
            return set(key, MValue());
        }

        /** Removes all items from the dictionary. */
        bool clear() {
            if (!MCollection::isMutable())
                return false;
            if (_count == 0)
                return true;
            MCollection::mutate();
            _map.clear();
            for (Dict::iterator i(_dict, sharedKeys()); i; ++i)
                _map.emplace(i.keyString(), MValue::empty);
            _count = 0;
            return true;
        }

        using iterator = MDictIterator<Native>;     // defined in MDictIterator.hh

        /** Writes the dictionary to an Encoder as a single Value. */
        void encodeTo(Encoder &enc) const {
            if (!MCollection::isMutated()) {
                enc << _dict;
            } else {
                enc.beginDict(count());
                for (iterator i(*this); i; ++i) {
                    enc.writeKey(i.key());
                    if (i.value())
                        enc << i.value();
                    else
                        i.mvalue().encodeTo(enc);
                }
                enc.endDict();
            }
        }

        /** The FLSharedKeys used for encoding dictionary keys. */
        FLSharedKeys sharedKeys() const     {return MCollection::context()->sharedKeys();}

    private:
        typename MDict::MapType::iterator _setInMap(slice key, const MValue &val) {
            _newKeys.emplace_back(key);
            key = _newKeys.back();
            return _map.emplace(key, val).first;
        }

        Dict                     _dict;     // Base Fleece dict (if any)
        MapType                  _map;      // Maps changed keys --> MValues
        std::vector<alloc_slice> _newKeys;  // storage for new key slices for _map
        uint32_t                 _count {0};// Current count

        friend MDictIterator<Native>;
    };

}
