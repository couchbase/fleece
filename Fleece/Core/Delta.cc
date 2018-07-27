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
#include "diff_match_patch.hh"
#include <sstream>
#include <unordered_set>


namespace fleece {
    using namespace std;


    bool gCompatibleDeltas = false;

    static const size_t kMinStringDiffLength = 60;

    static const float kTextDiffTimeout = 0.5;

    // Codes that appear as the 3rd item of an array item in a diff
    enum {
        kDeletionCode = 0,
        kTextDiffCode = 2,
        kArraymoveCode = 3,
    };


#pragma mark - STRING DELTAS:


    static string createStringDelta(slice oldStr, slice nuuStr) {
        if (nuuStr.size < kMinStringDiffLength
                || (gCompatibleDeltas && oldStr.size > kMinStringDiffLength))
            return "";
        diff_match_patch<string> dmp;
        dmp.Diff_Timeout = kTextDiffTimeout;
        auto patches = dmp.patch_make(string(oldStr), string(nuuStr));

        if (gCompatibleDeltas)
            return dmp.patch_toText(patches);

        long pos = 0, lastPos = 0, correction = 0;
        stringstream str;
        for (auto patch = patches.begin(); patch != patches.end(); ++patch) {
            pos = patch->start1 + correction;
            auto &diffs = patch->diffs;
            for (auto cur_diff = diffs.begin(); cur_diff != diffs.end(); ++cur_diff) {
                string &text = cur_diff->text;
                auto length = text.length();
                if (cur_diff->operation == diff_match_patch<string>::EQUAL) {
                    pos += length;
                } else {
                    if (pos > lastPos) {
                        str << (pos-lastPos) << '=';
                    }
                    if (cur_diff->operation == diff_match_patch<string>::DELETE) {
                        str << length << '-';
                        pos += length;
                    } else {
                        str << length << '+' << text << '|';
                    }
                    lastPos = pos;
                }
                if ((off_t)str.tellp() + 6 >= nuuStr.size)
                    return "";          // Patch is too long; send new str instead
            }
            correction += patch->length1 - patch->length2;
        }
        if (oldStr.size > lastPos)
            str << (oldStr.size-lastPos) << '=';
        return str.str();
    }


    static string applyStringDelta(slice oldStr, slice diff) {
        if (diff[0] == '@') {
            diff_match_patch<string> dmp;
            return dmp.patch_apply(dmp.patch_fromText(string(diff)), string(oldStr)).first;
        } else {
            stringstream in{string(diff)};
            in.exceptions(stringstream::failbit | stringstream::badbit);
            stringstream out;
            unsigned pos = 0;
            while (in.peek() >= 0) {
                char op;
                unsigned len;
                in >> len;
                in >> op;
                switch (op) {
                    case '=':
                        if (pos + len > oldStr.size)
                            FleeceException::_throw(InvalidData, "Invalid length in text delta");
                        out.write((const char*)&oldStr[pos], len);
                        pos += len;
                        break;
                    case '-':
                        pos += len;
                        break;
                    case '+': {
                        char insertion[len];
                        in.read(insertion, len);
                        out.write(insertion, len);
                        in >> op;
                        if (op != '|')
                            FleeceException::_throw(InvalidData, "Missing insertion delimiter in text delta");
                        break;
                    }
                    default:
                        FleeceException::_throw(InvalidData, "Unknown op in text delta");
                }
            }
            if (pos != oldStr.size)
                FleeceException::_throw(InvalidData, "Length mismatch in text delta");

            return out.str();
        }
    }


#pragma mark - CREATING DELTAS:


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


    static bool writeDelta(const Value *old, SharedKeys *oldSK,
                           const Value *nuu, SharedKeys *nuuSK,
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
            return true;

        } else if (!nuu) {
            // `old` was deleted:
            writePath(path, enc);
            enc.beginArray();
            if (gCompatibleDeltas) {
                enc.writeValue(old);
                enc.writeInt(0);
                enc.writeInt(kDeletionCode);
            }
            enc.endArray();
            return true;
        }

        auto oldType = old->type(), nuuType = nuu->type();
        if (oldType == nuuType) {
            if (oldType == kDict) {
                // Possibly-modified dict:
                auto oldDict = (const Dict*)old, nuuDict = (const Dict*)nuu;
                pathItem curLevel = {path, false, nullslice};
                unsigned oldKeysSeen = 0;
                // Iterate all the new & maybe-changed keys:
                for (Dict::iterator i_nuu(nuuDict, nuuSK); i_nuu; ++i_nuu) {
                    slice key = i_nuu.keyString();
                    auto oldValue = oldDict->get(key, oldSK);
                    if (oldValue)
                        ++oldKeysSeen;
                    curLevel.key = key;
                    writeDelta(oldValue, oldSK, i_nuu.value(), nuuSK, enc, &curLevel);
                }
                // Iterate all the deleted keys:
                if (oldKeysSeen < oldDict->count()) {
                    for (Dict::iterator i_old(oldDict, oldSK); i_old; ++i_old) {
                        slice key = i_old.keyString();
                        if (nuuDict->get(key, nuuSK) == nullptr) {
                            curLevel.key = key;
                            writeDelta(i_old.value(), oldSK, nullptr, nuuSK, enc, &curLevel);
                        }
                    }
                }
                if (!curLevel.isOpen)
                    return false;
                enc.endDictionary();
                return true;

            } else if (old->isEqual(nuu)) {
                // Equal objects: do nothing
                return false;

            } else if (oldType == kString && nuuType == kString) {
                // Strings: Maybe use smart text diff
                string strPatch = createStringDelta(old->asString(), nuu->asString());
                if (!strPatch.empty()) {
                    enc.beginArray();
                    enc.writeString(strPatch);
                    enc.writeInt(0);
                    enc.writeInt(kTextDiffCode);
                    enc.endArray();
                    return true;
                }
            }
        }

        // Generic modification:
        writePath(path, enc);
        enc.beginArray();
        if (gCompatibleDeltas)
            enc.writeValue(old);
        else
            enc.writeInt(0);    // deviating from the original, we don't write the old value
        enc.writeValue(nuu);
        enc.endArray();
        return true;
    }


    bool CreateDelta(const Value *old, SharedKeys *oldSK,
                     const Value *nuu, SharedKeys *nuuSK,
                     JSONEncoder &enc)
    {
        return writeDelta(old, oldSK, nuu, nuuSK, enc, nullptr);
    }

    alloc_slice CreateDelta(const Value *old, SharedKeys *oldSK,
                            const Value *nuu, SharedKeys *nuuSK,
                            bool json5) {
        JSONEncoder enc;
        enc.setJSON5(json5);
        if (writeDelta(old, oldSK, nuu, nuuSK, enc, nullptr))
            return enc.extractOutput();
        else
            return {};
    }


#pragma mark - APPLYING DELTAS:


    // Does this delta represent a deletion?
    static bool isDeltaDeletion(const Value *delta) {
        if (!delta)
            return false;
        auto array = delta->asArray();
        if (!array)
            return false;
        auto count = array->count();
        return count == 0 || (count == 3 && array->get(2)->asInt() == kDeletionCode);
    }


    void ApplyDelta(const Value *old, SharedKeys *sk, const Value *delta, Encoder &enc) {
        switch(delta->type()) {
            case kArray: {
                // Array: Insertion / deletion / replacement
                auto a = (const Array*)delta;
                switch (a->count()) {
                    case 0:
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
                    case 3: {
                        switch(a->get(2)->asInt()) {
                            case kDeletionCode:
                                if (!old)
                                    FleeceException::_throw(InvalidData, "Invalid deletion in delta");
                                enc.writeValue(Value::kUndefinedValue);
                                break;
                            case kTextDiffCode: {
                                slice oldStr = old->asString();
                                if (!oldStr)
                                    FleeceException::_throw(InvalidData, "Invalid text replace in delta");
                                slice diff = a->get(0)->asString();
                                if (diff.size == 0)
                                    FleeceException::_throw(InvalidData, "Invalid text diff in delta");
                                auto nuuStr = applyStringDelta(oldStr, diff);
                                enc.writeString(nuuStr);
                                break;
                            }
                            default:
                                FleeceException::_throw(InvalidData, "Unknown mode in delta");
                        }
                        break;
                    }
                    default:
                        FleeceException::_throw(InvalidData, "Bad array count in delta");
                }
                break;
            }
            case kDict: {
                // Dict: Incremental update
                auto deltaDict = (const Dict*)delta;
                auto oldDict = old ? old->asDict() : nullptr;
                if (!oldDict)
                    FleeceException::_throw(InvalidData, "Invalid {} in delta");
                if (enc.valueIsInBase(oldDict)) {
                    // If the old dict is in the base, we can create an inherited dict:
                    enc.beginDictionary(oldDict);
                    for (Dict::iterator i(deltaDict); i; ++i) {
                        slice key = i.keyString();
                        enc.writeKey(key);
                        ApplyDelta(oldDict->get(key, sk), sk, i.value(), enc);  // recurse into dict item!
                    }
                    enc.endDictionary();
                } else {
                    // In the general case, have to write a new dict from scratch:
                    enc.beginDictionary();
                    // Process the unaffected, deleted, and modified keys:
                    unsigned deltaKeysUsed = 0;
                    for (Dict::iterator i(oldDict, sk); i; ++i) {
                        slice key = i.keyString();
                        const Value *valueDelta = deltaDict->get(key);
                        if (valueDelta)
                            ++deltaKeysUsed;
                        if (!isDeltaDeletion(valueDelta)) {                 // skip deletions
                            enc.writeKey(key);
                            auto oldValue = i.value();
                            if (valueDelta == nullptr)
                                enc.writeValue(oldValue);                   // unaffected
                            else
                                ApplyDelta(oldValue, sk, valueDelta, enc);  // replaced/modified
                        }
                    }
                    // Now add the inserted keys:
                    if (deltaKeysUsed < deltaDict->count()) {
                        for (Dict::iterator i(deltaDict); i; ++i) {
                            slice key = i.keyString();
                            if (oldDict->get(key, sk) == nullptr) {
                                enc.writeKey(key);
                                ApplyDelta(nullptr, sk, i.value(), enc);  // recurse into insertion
                            }
                        }
                    }
                    enc.endDictionary();
                }
                break;
            }
            default:
                FleeceException::_throw(InvalidData, "Invalid value type in delta");
        }
    }


    alloc_slice ApplyDelta(const Value *old, SharedKeys *sk, slice jsonDelta, bool isJSON5) {
        assert(jsonDelta);
        string json5;
        if (isJSON5) {
            json5 = ConvertJSON5(string(jsonDelta));
            jsonDelta = slice(json5);
        }
        alloc_slice fleeceData = JSONConverter::convertJSON(jsonDelta);
        const Value *fleeceDelta = Value::fromTrustedData(fleeceData);
        Encoder enc;
        ApplyDelta(old, sk, fleeceDelta, enc);
        return enc.extractOutput();
    }

}
