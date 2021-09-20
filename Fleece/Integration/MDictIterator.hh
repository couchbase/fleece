//
// MDictIterator.hh
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "MDict.hh"

namespace fleece {

    template <class Native>
        class MDictIterator {
        public:
            using MDict = MDict<Native>;
            using MValue = MValue<Native>;

            MDictIterator(const MDict &dict)
            :_dict(dict)
            ,_mapIter(dict._map.begin())
            ,_mapEnd(dict._map.end())
            ,_dictIter(dict._dict)
            {
                read(); // find first k/v
            }

            operator bool() const       {return (bool)_key;}

            MDictIterator& operator++() {
                if (_iteratingMap)
                    ++_mapIter;
                else
                    ++_dictIter;
                read();
                return *this;
            }

            slice key() const       {return _key;}
            Value value() const     {return _mvalue ? _mvalue->value() : _dictIter.value();}

            const MValue& mvalue() {
                if (_mvalue)
                    return *_mvalue;
                // Fleece Dict iterator doesn't have an MValue, so add the key/value to the _map:
                auto i = const_cast<MDict&>(_dict)._setInMap(_key, MValue(_dictIter.value()));
                _mvalue = &i->second;
                return *_mvalue;
            }

            Native nativeKey() const;

            Native nativeValue()  {
                return mvalue().asNative(&_dict);
            }


        private:
            void read() {
                while (_iteratingMap) {
                    // Skip tombstones in the _map:
                    if (_mapIter == _mapEnd) {
                        _iteratingMap = false;      // Ran out of map entries; move on to Dict
                    } else if (!_mapIter->second.isEmpty()) {
                        _key = _mapIter->first;
                        _mvalue = &_mapIter->second;
                        return;                     // found an item in the map
                    } else {
                        ++_mapIter;
                    }
                }
                _mvalue = nullptr;

                while (_dictIter) {
                    // Skip overwritten keys in the original Fleece Dict:
                    if (_dict._map.find(_dictIter.keyString()) == _mapEnd) {
                        _key = slice(_dictIter.keyString());
                        return;         // found an item in _dict
                    }
                    ++_dictIter;
                }
                // I got nuthin'; mark iteration as complete:
                _key = fleece::nullslice;
            }

            using MapIterator = typename MDict::MapType::const_iterator;

            const MDict&    _dict;                  // MDict being iterated
            MapIterator     _mapIter, _mapEnd;      // Map iterator, and end of map
            Dict::iterator  _dictIter;              // Fleece Dict iterator; used after end of map
            slice           _key;                   // Current key
            const MValue*   _mvalue {nullptr};      // Current MValue, if in _map
            bool            _iteratingMap {true};   // Still iterating map?
        };

}
