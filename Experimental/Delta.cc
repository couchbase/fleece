//
// Delta.cc
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

// Delta format: <https://github.com/benjamine/jsondiffpatch/blob/master/docs/deltas.md>
// This implementation differs from that reference one:
// * When it encodes a modification or deletion, it does not write the old value; it leaves a '0'
//   in its place instead.
//   The reason is that we don't need the old value to apply the delta, and it can make the delta
//   data much larger.
// * There is no special handling of array diffs (yet).
// * There is no special handling of text diffs (yet).

#include "Delta.hh"
#include "Fleece.hh"
#include "JSONEncoder.hh"
#include "JSONConverter.hh"
#include "JSON5.hh"
#include "FleeceException.hh"
#include <unordered_set>


namespace fleece {


    bool gCompatibleDeltas = false;


    struct pathItem {
        pathItem *parent;
        bool isOpen;
        slice key;
    };


    static void writePath(pathItem *path, JSONEncoder &enc) {
        if (!path)
            return;
        writePath(path->parent, enc);
        path->parent = nullptr;
        if (!path->isOpen) {
            enc.beginDictionary();
            path->isOpen = true;
        }
        enc.writeKey(path->key);
    }


    static bool writeDelta(const Value *old, const Value *nuu,
                           JSONEncoder &enc,
                           pathItem *path)
    {
        if (!old) {
            // `nuu` was added:
            if (!nuu)
                return false;
            writePath(path, enc);
            enc.beginArray();
            enc.writeValue(nuu);
            enc.endArray();

        } else if (!nuu) {
            // `old` was deleted:
            writePath(path, enc);
            enc.beginArray();
            if (gCompatibleDeltas) {
                enc.writeValue(old);
                enc.writeInt(0);
                enc.writeInt(0);
            }
            enc.endArray();

        } else if (old->type() == kDict && nuu->type() == kDict) {
            // Possibly-modified dict:
            auto oldDict = (const Dict*)old, nuuDict = (const Dict*)nuu;
            pathItem curLevel = {path, false, nullslice};
            unsigned oldKeysSeen = 0;
            // Iterate all the new & maybe-changed keys:
            for (Dict::iterator i_nuu(nuuDict); i_nuu; ++i_nuu) {
                slice key = i_nuu.keyString();
                auto oldValue = oldDict->get(key);
                if (oldValue)
                    ++oldKeysSeen;
                curLevel.key = key;
                writeDelta(oldValue, i_nuu.value(), enc, &curLevel);
            }
            // Iterate all the deleted keys:
            if (oldKeysSeen < oldDict->count()) {
                for (Dict::iterator i_old(oldDict); i_old; ++i_old) {
                    slice key = i_old.keyString();
                    if (nuuDict->get(key) == nullptr) {
                        curLevel.key = key;
                        writeDelta(i_old.value(), nullptr, enc, &curLevel);
                    }
                }
            }
            if (!curLevel.isOpen)
                return false;
            enc.endDictionary();

        } else if (old->isEqual(nuu)) {
            // Equal objects: do nothing
            return false;

        } else {
            // Generic modification:
            writePath(path, enc);
            enc.beginArray();
            if (gCompatibleDeltas)
                enc.writeValue(old);
            else
                enc.writeInt(0);    // deviating from the original, we don't write the old value
            enc.writeValue(nuu);
            enc.endArray();
        }
        return true;
    }


    bool CreateDelta(const Value *old, const Value *nuu, JSONEncoder &enc) {
        return writeDelta(old, nuu, enc, nullptr);
    }

    alloc_slice CreateDelta(const Value* old, const Value* nuu, bool json5) {
        JSONEncoder enc;
        enc.setJSON5(json5);
        if (writeDelta(old, nuu, enc, nullptr))
            return enc.extractOutput();
        else
            return {};
    }


    void ApplyDelta(const Value *old, const Value *delta, Encoder &enc) {
        switch(delta->type()) {
            case kArray: {
                // Array: Insertion / deletion / replacement
                auto a = (const Array*)delta;
                switch (a->count()) {
                    case 0:
                    case 3:
                        if (!old)
                            FleeceException::_throw(InvalidData, "Invalid deletion in delta");
                        // 'undefined' in the context of a dict value means a deletion of a key
                        // inherited from the parent.
                        enc.writeValue(Value::kUndefinedValue);
                        break;
                    case 1:
                        if (old)
                            FleeceException::_throw(InvalidData, "Invalid insertion in delta");
                        enc.writeValue(a->get(0));
                        break;
                    case 2:
                        if (!old)
                            FleeceException::_throw(InvalidData, "Invalid replace in delta");
                        enc.writeValue(a->get(1));
                        break;
                    default:
                        FleeceException::_throw(InvalidData, "Bad array count in delta");
                }
                break;
            }
            case kDict: {
                // Dict: Incremental update
                auto d = (const Dict*)delta;
                auto oldDict = old->asDict();
                if (!oldDict)
                    FleeceException::_throw(InvalidData, "Invalid {} in delta");
                enc.beginDictionary(oldDict);   // inherit from oldDict
                for (Dict::iterator i(d); i; ++i) {
                    slice key = i.keyString();
                    enc.writeKey(key);
                    ApplyDelta(oldDict->get(key), i.value(), enc);  // recurse into dict item!
                }
                enc.endDictionary();
                break;
            }
            default:
                FleeceException::_throw(InvalidData, "Invalid value type in delta");
        }
    }


    alloc_slice ApplyDelta(const Value *old, slice jsonDelta, bool isJSON5) {
        assert(jsonDelta);
        std::string json5;
        if (isJSON5) {
            json5 = ConvertJSON5(std::string(jsonDelta));
            jsonDelta = slice(json5);
        }
        alloc_slice fleeceData = JSONConverter::convertJSON(jsonDelta);
        const Value *fleeceDelta = Value::fromTrustedData(fleeceData);
        Encoder enc;
        ApplyDelta(old, fleeceDelta, enc);
        return enc.extractOutput();
    }

}
