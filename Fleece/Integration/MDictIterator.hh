//
// MDictIterator.hh
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
#include "MDict.hh"

namespace fleeceapi {

    template <class Native>
        class MDictIterator {
        public:
            using MDict = MDict<Native>;
            using MValue = MValue<Native>;

            MDictIterator(const MDict &dict)
            :_dict(dict)
            ,_mapIter(dict._map.begin())
            ,_mapEnd(dict._map.end())
            ,_dictIter(dict._dict, _dict.sharedKeys())
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
