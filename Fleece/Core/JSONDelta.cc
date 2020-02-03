//
// JSONDelta.cc
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

#include "JSONDelta.hh"
#include "FleeceImpl.hh"
#include "JSONEncoder.hh"
#include "JSONConverter.hh"
#include "JSON5.hh"
#include "FleeceException.hh"
#include "TempArray.hh"
#include "diff_match_patch.hh"
#include <sstream>
#include <unordered_set>
#include "betterassert.hh"


namespace fleece { namespace impl {
    using namespace std;


    // Set this to true to create deltas compatible with JsonDiffPatch.
    // (this is really just here for test purposes so we can use the JDP unit test dataset...)
    bool gCompatibleDeltas = false;

    size_t JSONDelta::gMinStringDiffLength = 60;

    float JSONDelta::gTextDiffTimeout = 0.25;

    // Codes that appear as the 3rd item of an array item in a diff
    enum {
        kDeletionCode = 0,
        kTextDiffCode = 2,
        kArraymoveCode = 3,
    };


    // Is `c` the 2nd, 3rd, ... byte of a UTF-8 multibyte character?
    // <https://en.wikipedia.org/wiki/UTF-8#Description>
    static inline bool isUTF8Continuation(uint8_t c) {
        return (c & 0xc0) == 0x80;
    }


    static void snapToUTF8Character(long &pos, size_t &length, slice str);


#pragma mark - CREATING DELTAS:


    /*static*/ alloc_slice JSONDelta::create(const Value *old, const Value *nuu, bool json5) {
        JSONEncoder enc;
        enc.setJSON5(json5);
        create(old, nuu, enc);
        return enc.finish();
    }


    /*static*/ bool JSONDelta::create(const Value *old, const Value *nuu, JSONEncoder &enc) {
        if (JSONDelta(enc)._write(old, nuu, nullptr))
            return true;
        // If there is no difference, write a no-op delta:
        enc.beginDictionary();
        enc.endDictionary();
        return false;
    }


    JSONDelta::JSONDelta(JSONEncoder &enc)
    :_encoder(&enc)
    { }


    struct JSONDelta::pathItem {
        pathItem *parent;
        bool isOpen;
        slice key;
    };


    void JSONDelta::writePath(pathItem *path) {
        if (!path)
            return;
        writePath(path->parent);
        path->parent = nullptr;
        if (!path->isOpen) {
            _encoder->beginDictionary();
            path->isOpen = true;
        }
        _encoder->writeKey(path->key);
    }


    // Main encoder function. Called recursively, traversing the hierarchy.
    bool JSONDelta::_write(const Value *old, const Value *nuu, pathItem *path) {
        if (_usuallyFalse(old == nuu))
            return false;
        if (old) {
            if (!nuu) {
                // `old` was deleted: write []
                writePath(path);
                _encoder->beginArray();
                if (gCompatibleDeltas) {
                    _encoder->writeValue(old);
                    _encoder->writeInt(0);
                    _encoder->writeInt(kDeletionCode);
                }
                _encoder->endArray();
                return true;
            }

            auto oldType = old->type(), nuuType = nuu->type();
            if (oldType == nuuType) {
                if (oldType == kDict) {
                    // Possibly-modified dict: write a dict with the modified keys
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
                        _write(oldValue, i_nuu.value(), &curLevel);
                    }
                    // Iterate all the deleted keys:
                    if (oldKeysSeen < oldDict->count()) {
                        for (Dict::iterator i_old(oldDict); i_old; ++i_old) {
                            slice key = i_old.keyString();
                            if (nuuDict->get(key) == nullptr) {
                                curLevel.key = key;
                                _write(i_old.value(), nullptr, &curLevel);
                            }
                        }
                    }
                    if (!curLevel.isOpen)
                        return false;
                    _encoder->endDictionary();
                    return true;

                } else if (oldType == kArray) {
                    // Scan forwards through unchanged items:
                    auto oldArray = (const Array*)old, nuuArray = (const Array*)nuu;
                    auto oldCount = oldArray->count(), nuuCount = nuuArray->count();
                    auto minCount = min(oldCount, nuuCount);
                    if (minCount > 0) {
                        pathItem curLevel = {path, false, nullslice};
                        uint32_t index = 0;
                        char key[10];
                        for (Array::iterator iOld(oldArray), iNew(nuuArray); index < minCount;
                             ++iOld, ++iNew, ++index) {
                            sprintf(key, "%d", index);
                            curLevel.key = slice(key);
                            _write(iOld.value(), iNew.value(), &curLevel);
                        }
                        if (oldCount != nuuCount) {
                            sprintf(key, "%d-", index);
                            curLevel.key = slice(key);
                            writePath(&curLevel);
                            _encoder->beginArray();
                            for (; index < nuuCount; ++index) {
                                _encoder->writeValue(nuuArray->get(index));
                            }
                            _encoder->endArray();
                        }
                        if (!curLevel.isOpen)
                            return false;
                        _encoder->endDictionary();
                        return true;
                    } else if (oldCount == 0 && nuuCount == 0) {
                        return false;
                    }

                } else if (old->isEqual(nuu)) {
                    // Equal objects: do nothing
                    return false;

                } else if (oldType == kString && nuuType == kString) {
                    // Strings: Try to use smart text diff
                    string strPatch = createStringDelta(old->asString(), nuu->asString());
                    if (!strPatch.empty()) {
                        writePath(path);
                        _encoder->beginArray();
                        _encoder->writeString(strPatch);
                        _encoder->writeInt(0);
                        _encoder->writeInt(kTextDiffCode);
                        _encoder->endArray();
                        return true;
                    }
                    // if there's no smart diff, fall through to the generic case...
                }
            }
        }

        // Generic modification/insertion:
        writePath(path);
        if (nuu->type() < kArray && path && !gCompatibleDeltas) {
            _encoder->writeValue(nuu);
        } else {
            _encoder->beginArray();
            if (gCompatibleDeltas && old)
                _encoder->writeValue(old);
            _encoder->writeValue(nuu);
            _encoder->endArray();
        }
        return true;
    }


#pragma mark - APPLYING DELTAS:


    /*static*/ alloc_slice JSONDelta::apply(const Value *old, slice jsonDelta, bool isJSON5) {
        Encoder enc;
        apply(old, jsonDelta, isJSON5, enc);
        return enc.finish();
    }


    /*static*/ void JSONDelta::apply(const Value *old, slice jsonDelta, bool isJSON5, Encoder &enc) {
        assert_precondition(jsonDelta);
        string json5;
        if (isJSON5) {
            json5 = ConvertJSON5(string(jsonDelta));
            jsonDelta = slice(json5);
        }

        // Parse JSON delta to Fleece using same SharedKeys as `old`:
        auto sk = old->sharedKeys();
        alloc_slice fleeceData = JSONConverter::convertJSON(jsonDelta, sk);
        Scope scope(fleeceData, sk);
        const Value *fleeceDelta = Value::fromTrustedData(fleeceData);

        JSONDelta(enc)._apply(old, fleeceDelta);
    }


    JSONDelta::JSONDelta(Encoder &decoder)
    :_decoder(&decoder)
    { }


    // Recursively applies the delta to the value, going down into the tree
    void JSONDelta::_apply(const Value *old, const Value *delta) {
        switch(delta->type()) {
            case kArray:
                _applyArray(old, (const Array*)delta);
                break;
            case kDict: {
                auto deltaDict = (const Dict*)delta;
                switch (old ? old->type() : kNull) {
                    case kArray:
                        _patchArray((const Array*)old, deltaDict);
                        break;
                    case kDict:
                        _patchDict((const Dict*)old, deltaDict);
                        break;
                    default:
                        if (deltaDict->empty() && old)
                            _decoder->writeValue(old);
                        else
                            FleeceException::_throw(InvalidData, "Invalid {...} in delta");
                }
                break;
            }
            default:
                _decoder->writeValue(delta);
                break;
        }
    }


    inline void JSONDelta::_applyArray(const Value *old, const Array* NONNULL delta) {
        switch (delta->count()) {
            case 0:
                // Deletion:
                throwIf(!old, InvalidData, "Invalid deletion in delta");
                // 'undefined' in the context of a dict value means a deletion of a key
                // inherited from the parent.
                _decoder->writeValue(Value::kUndefinedValue);
                break;
            case 1:
                // Insertion / replacement:
                _decoder->writeValue(delta->get(0));
                break;
            case 2:
                // Replacement (JsonDiffPatch format):
                throwIf(!old, InvalidData, "Invalid replace in delta");
                _decoder->writeValue(delta->get(1));
                break;
            case 3: {
                switch(delta->get(2)->asInt()) {
                    case kDeletionCode:
                        // JsonDiffPatch deletion:
                        throwIf(!old, InvalidData, "Invalid deletion in delta");
                        _decoder->writeValue(Value::kUndefinedValue);
                        break;
                    case kTextDiffCode: {
                        // Text diff:
                        slice oldStr;
                        if (old)
                            oldStr = old->asString();
                        throwIf(!oldStr, InvalidData, "Invalid text replace in delta");
                        slice diff = delta->get(0)->asString();
                        throwIf(diff.size == 0, InvalidData, "Invalid text diff in delta");
                        auto nuuStr = applyStringDelta(oldStr, diff);
                        _decoder->writeString(nuuStr);
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
    }


    inline void JSONDelta::_patchDict(const Dict* NONNULL old, const Dict* NONNULL delta) {
        // Dict: Incremental update
        if (_decoder->valueIsInBase(old)) {
            // If the old dict is in the base, we can create an inherited dict:
            _decoder->beginDictionary(old);
            for (Dict::iterator i(delta); i; ++i) {
                _decoder->writeKey(i.keyString());
                _apply(old->get(i.key()), i.value());  // recurse into dict item!
            }
            _decoder->endDictionary();
        } else {
            // In the general case, have to write a new dict from scratch:
            _decoder->beginDictionary();
            // Process the unaffected, deleted, and modified keys:
            unsigned deltaKeysUsed = 0;
            for (Dict::iterator i(old); i; ++i) {
                const Value *valueDelta = delta->get(i.key());
                if (valueDelta)
                    ++deltaKeysUsed;
                if (!isDeltaDeletion(valueDelta)) {                 // skip deletions
                    _decoder->writeKey(i.keyString());
                    auto oldValue = i.value();
                    if (valueDelta == nullptr)
                        _decoder->writeValue(oldValue);               // unaffected
                    else
                        _apply(oldValue, valueDelta);  // replaced/modified
                }
            }
            // Now add the inserted keys:
            if (deltaKeysUsed < delta->count()) {
                for (Dict::iterator i(delta); i; ++i) {
                    if (old->get(i.key()) == nullptr) {
                        _decoder->writeKey(i.keyString());
                        _apply(nullptr, i.value());  // recurse into insertion
                    }
                }
            }
            _decoder->endDictionary();
        }
    }


    inline void JSONDelta::_patchArray(const Array* NONNULL old, const Dict* NONNULL delta) {
        // Array: Incremental update
        _decoder->beginArray();
        uint32_t index = 0;
        const Value *remainder = nullptr;
        for (Array::iterator iOld(old); iOld; ++iOld, ++index) {
            auto oldItem = iOld.value();
            char key[10];
            sprintf(key, "%d", index);
            auto replacement = delta->get(slice(key));
            if (replacement) {
                // Patch this array item:
                _apply(oldItem, replacement);
            } else {
                strcat(key, "-");
                remainder = delta->get(slice(key));
                if (remainder) {
                    break;
                } else {
                    // Array item is unaffected:
                    _decoder->writeValue(oldItem);
                }
            }
        }

        if (!remainder) {
            char key[10];
            sprintf(key, "%d-", old->count());
            remainder = delta->get(slice(key));
        }
        if (remainder) {
            // Remainder of array is replaced by the array from the delta:
            auto remainderArray = remainder->asArray();
            throwIf(!remainderArray, InvalidData, "Invalid array remainder in delta");
            for (Array::iterator iRem(remainderArray); iRem; ++iRem)
                _decoder->writeValue(iRem.value());
        }
        _decoder->endArray();
    }


    // Does this delta represent a deletion?
    /*static*/ inline bool JSONDelta::isDeltaDeletion(const Value *delta) {
        if (!delta)
            return false;
        auto array = delta->asArray();
        if (!array)
            return false;
        auto count = array->count();
        return count == 0 || (count == 3 && array->get(2)->asInt() == kDeletionCode);
    }


#pragma mark - STRING DELTAS:


    /*static*/ string JSONDelta::createStringDelta(slice oldStr, slice nuuStr) {
        if (nuuStr.size < gMinStringDiffLength
                || (gCompatibleDeltas && oldStr.size > gMinStringDiffLength))
            return "";
        diff_match_patch<string> dmp;
        dmp.Diff_Timeout = gTextDiffTimeout;
        auto patches = dmp.patch_make(string(oldStr), string(nuuStr));

        if (gCompatibleDeltas)
            return dmp.patch_toText(patches);

        // Iterate over the diffs, writing the encoded form to the output stream:
        stringstream diff;
        long lastOldPos = 0, correction = 0;
        for (auto patch = patches.begin(); patch != patches.end(); ++patch) {
            long oldPos = patch->start1 + correction;   // position in oldStr
            long nuuPos = patch->start2;                // position in nuuStr
            auto &diffs = patch->diffs;
            for (auto cur_diff = diffs.begin(); cur_diff != diffs.end(); ++cur_diff) {
                auto length = cur_diff->text.length();
                if (cur_diff->operation == diff_match_patch<string>::EQUAL) {
                    oldPos += length;
                    nuuPos += length;
                } else {
                    // Don't break up a UTF-8 multibyte character:
                    if (cur_diff->operation == diff_match_patch<string>::DELETE)
                        snapToUTF8Character(oldPos, length, oldStr);
                    else
                        snapToUTF8Character(nuuPos, length, nuuStr);

                    assert(oldPos >= lastOldPos);
                    if (oldPos > lastOldPos) {
                        // Write the number of matching bytes since the last insert/delete:
                        diff << (oldPos-lastOldPos) << '=';
                    }
                    if (cur_diff->operation == diff_match_patch<string>::DELETE) {
                        // Write the number of deleted bytes:
                        diff << length << '-';
                        oldPos += length;
                    } else /* INSERT */ {
                        // Write an insertion, both the count and the bytes:
                        diff << length << '+';
                        diff.write((const char*)&nuuStr[nuuPos], length);
                        diff << '|';
                        nuuPos += length;
                    }
                    lastOldPos = oldPos;
                }
                if ((size_t)diff.tellp() + 6 >= nuuStr.size)
                    return "";          // Patch is too long; give up on using a diff
            }
            correction += patch->length1 - patch->length2;
        }
        if (oldStr.size > size_t(lastOldPos)) {
            // Write a final matching-bytes count:
            diff << (oldStr.size-lastOldPos) << '=';
        }
        return diff.str();
    }


    /*static*/ string JSONDelta::applyStringDelta(slice oldStr, slice diff) {
#if 0
        // Support for JsonDiffPatch-format string diffs:
        if (diff[0] == '@') {
            diff_match_patch<string> dmp;
            return dmp.patch_apply(dmp.patch_fromText(string(diff)), string(oldStr)).first;
        }
#endif

        stringstream in{string(diff)};
        in.exceptions(stringstream::failbit | stringstream::badbit);
        stringstream nuu;
        unsigned pos = 0;
        while (in.peek() >= 0) {
            char op;
            unsigned len;
            in >> len;
            in >> op;
            switch (op) {
                case '=':
                    throwIf(pos + len > oldStr.size, InvalidData, "Invalid length in text delta");
                    nuu.write((const char*)&oldStr[pos], len);
                    pos += len;
                    break;
                case '-':
                    pos += len;
                    break;
                case '+': {
                    TempArray(insertion, char, len);
                    in.read(insertion, len);
                    nuu.write(insertion, len);
                    in >> op;
                    throwIf(op != '|', InvalidData, "Missing insertion delimiter in text delta");
                    break;
                }
                default:
                    FleeceException::_throw(InvalidData, "Unknown op in text delta");
            }
        }
        throwIf(pos != oldStr.size, InvalidData, "Length mismatch in text delta");
        return nuu.str();
    }


    // Given a byte range (`pos`, `length`) in the slice `str`, if either end of the range falls
    // in the middle of a UTF-8 multibyte character, push it _outwards_ to include the entire char.
    static void snapToUTF8Character(long &pos, size_t &length, slice str) {
        while (isUTF8Continuation(str[pos])) {
            --pos;
            throwIf(pos < 0, InvalidData, "Invalid UTF-8 at start of a string");
            ++length;
        }
        while (pos+length < str.size && isUTF8Continuation(str[pos+length])) {
            ++length;
        }
    }

} }
