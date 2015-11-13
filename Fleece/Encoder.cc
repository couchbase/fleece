//
//  Encoder.cc
//  Fleece
//
//  Created by Jens Alfke on 1/26/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#include "Encoder.hh"
#include "Endian.h"
#include "varint.hh"
#include <assert.h>


namespace fleece {

    static size_t kMinSharedStringLength = 4, kMaxSharedStringLength = 100;

    encoder::encoder(Writer& out,
                           value::stringTable *externStrings,
                           uint32_t maxExternStrings)
    :_out(out),
     _enableSharedStrings(false),
     _externStrings(externStrings),
     _maxExternStrings(maxExternStrings)
    {
        // Invert externStrings, if given:
        if (externStrings) {
            for (size_t i = 0; i < externStrings->size(); i++)
                _externStringsLookup[externStrings->at(i)] = (uint32_t)i + 1;
        }
        pushState();
        _state->count = 0;
    }

    void encoder::addUVarint(uint64_t n) {
        char buf[kMaxVarintLen64];
        _out.write(buf, PutUVarInt(buf, n));
    }

    void encoder::writeNull() {
        addTypeCode(value::kNullCode);
    }

    void encoder::writeBool(bool b) {
        addTypeCode(b ? value::kTrueCode : value::kFalseCode);
    }

    void encoder::writeInt(int64_t i) {
        char buf[8];
        size_t size = PutIntOfLength(buf, i);
        addTypeCode(value::typeCode(value::kInt1Code + (size-1)));
        _out.write(buf, size);
    }

    void encoder::writeUInt(uint64_t u) {
        if (u < INT64_MAX)
            return writeInt((int64_t)u);
        u = _encLittle64(u);
        addTypeCode(value::kUInt64Code);
        _out.write((const char*)&u, 8);
    }

    void encoder::writeDouble(double n) {
        if (isnan(n))
            throw "Can't write NaN";
        if (n == (int64_t)n)
            return writeInt((int64_t)n);
        littleEndianDouble swapped = n;
        addTypeCode(value::kFloat64Code);
        _out.write((const char*)&swapped, 8);
    }

    void encoder::writeFloat(float n) {
        if (isnan(n))
            throw "Can't write NaN";
        if (n == (int32_t)n)
            return writeInt((int32_t)n);
        littleEndianFloat swapped = n;
        addTypeCode(value::kFloat32Code);
        _out.write((const char*)&swapped, 4);
    }

    void encoder::writeRawNumber(slice s) {
        addTypeCode(value::kRawNumberCode);
        addUVarint(s.size);
        _out << s;
    }

    void encoder::writeDate(std::time_t dateTime) {
        addTypeCode(value::kDateCode);
        addUVarint(dateTime);
    }

    void encoder::writeData(slice s) {
        addTypeCode(value::kDataCode);
        addUVarint(s.size);
        _out << s;
    }

    inline bool encoder::canShareString(size_t len) const {
        return _enableSharedStrings && len >= kMinSharedStringLength
                                    && len <= kMaxSharedStringLength;
    }

    void encoder::writeString(slice s, bool canAddExtern) {
        if (_externStrings || canShareString(s.size)) {
            return writeString(std::string(s), canAddExtern);
        } else {
            // not shareable or externable so no need to convert to std::string
            addTypeCode(value::kStringCode);
            addUVarint(s.size);
            _out << s;
        }
    }

    void encoder::writeString(std::string str, bool canAddExtern) {
        size_t len = str.length();
        if (_externStrings) {
            auto externID = _externStringsLookup.find(str);
            if (externID != _externStringsLookup.end()) {
                // Existing extern string:
                writeExternString(externID->second);
                return;
            }
            uint32_t n = (uint32_t)_externStrings->size();
            if (n < _maxExternStrings && canAddExtern) {
                // Add a new extern string:
                _externStrings->push_back(str);
                _externStringsLookup[str] = ++n;
                writeExternString(n);
                return;
            }
        }

        if (canShareString(len)) {
            size_t curOffset = _out.length();
            if (curOffset <= UINT32_MAX) {
                size_t sharedOffset = _sharedStrings[str];
                if (sharedOffset > 0) {
                    // Change previous string opcode to shared:
                    value::typeCode code = value::kSharedStringCode;
                    _out.rewrite(sharedOffset, slice(&code,sizeof(code)));

                    // Write reference to previous string:
                    addTypeCode(value::kSharedStringRefCode);
                    addUVarint(curOffset - sharedOffset);
                    return;
                }
                _sharedStrings[str] = (uint32_t)curOffset;
            }
        }

        // First appearance, or unshareable, so write the string itself:
        addTypeCode(value::kStringCode);
        addUVarint(len);
        _out << str;
    }

    void encoder::writeExternString(uint32_t externRef) {
        assert(externRef > 0);
        addTypeCode(value::kExternStringRefCode);
        addUVarint(externRef);
    }

    void encoder::pushState() {
        _states.push_back(state());
        _state = &_states[_states.size()-1];
        _state->hashes = NULL;
    }

    void encoder::popState() {
        if (_state->i != _state->count)
            throw "encoder: mismatched count";
        delete[] _state->hashes;
        _states.pop_back();
        _state = &_states[_states.size()-1];
    }

    void encoder::pushCount(uint32_t count) {
        addUVarint(count);
        pushState();
        _state->count = count;
        _state->i = 0;
    }

    void encoder::beginArray(uint32_t count) {
        addTypeCode(value::kArrayCode);
        pushCount(count);
    }

    void encoder::beginDict(uint32_t count) {
        addTypeCode(value::kDictCode);
        pushCount(count);
        // Write an empty/garbage hash list as a placeholder to fill in later:
        _state->hashes = new uint16_t[count];
        _state->indexPos = _out.length();
        _out.write((char*)_state->hashes, (std::streamsize)(_state->count*sizeof(uint16_t)));
    }

    void encoder::writeKey(std::string key, bool canAddExtern) {
        _state->hashes[_state->i] = dict::hashCode(key);
        writeString(key, canAddExtern);
        --_state->i; // the key didn't 'count' as a dict item
    }

    void encoder::writeKey(slice key, bool canAddExtern) {
        _state->hashes[_state->i] = dict::hashCode(key);
        writeString(key, canAddExtern);
        --_state->i; // the key didn't 'count' as a dict item
    }

    void encoder::writeExternKey(uint32_t externRef, uint16_t hash) {
        _state->hashes[_state->i] = hash;
        writeExternString(externRef);
        --_state->i; // the key didn't 'count' as a dict item
    }


    void encoder::endDict() {
        _out.rewrite(_state->indexPos, slice(_state->hashes, _state->count*sizeof(uint16_t)));
        popState();
    }

}
