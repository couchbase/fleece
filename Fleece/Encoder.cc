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
#include <algorithm>
#include <assert.h>


namespace fleece {

    // root encoder
    encoder::encoder(Writer& out)
    :_parent(NULL),
     _offset(0),
     _keyOffset(0),
     _count(1),
     _out(out),
     _blockedOnKey(false),
     _writingKey(false)
    { }

    encoder::encoder(encoder *parent, size_t offset, size_t keyOffset, size_t count)
    :_parent(parent),
     _offset(offset),
     _keyOffset(keyOffset),
     _count(count),
     _out(parent->_out),
     _blockedOnKey(keyOffset > 0),
     _writingKey(_blockedOnKey)
    { }

    void encoder::reset() {
        if (_parent)
            throw "can only reset root encoder";
        _count = 1;
        _out = Writer();
    }

    // primitive to write a value
    void encoder::writeValue(value::tags tag, uint8_t *buf, size_t size, bool canInline) {
        if (_count == 0)
            throw "no more space in collection";
        if (_blockedOnKey)
            throw "need a key before this value";
        size_t &offset = _writingKey ? _keyOffset : _offset;

        assert((buf[0] & 0xF0) == 0);
        buf[0] |= tag<<4;

        if (_parent) {
            if (size <= 2 && canInline) {
                // Add directly to parent collection at offset:
                _out.rewrite(offset, slice(buf,size));
            } else {
                // Write to output, then add a pointer in the parent collection:
                ssize_t delta = (_out.length() - offset) / 2;
                if (delta < -0x4000 || delta >= 0x4000)
                    throw "delta too large to write value";
                int16_t pointer = _enc16(delta | 0x8000);
                _out.rewrite(offset, slice(&pointer,2));
                _out.write(buf, size);
            }
            offset += 2;
        } else {
            // Root element: just write it
            _out.write(buf, size);
        }

        if (_writingKey) {
            _writingKey = false;
        } else {
            --_count;
            if (_keyOffset > 0)
                _blockedOnKey = _writingKey = true;
        }
    }

    inline void encoder::writeSpecial(uint8_t special) {
        uint8_t buf[2] = {0, special};
        writeValue(value::kSpecialTag, buf, 2);
    }

    void encoder::writeNull() {
        writeSpecial(value::kSpecialValueNull);
    }

    void encoder::writeBool(bool b) {
        writeSpecial(b ? value::kSpecialValueTrue : value::kSpecialValueFalse);
    }

    void encoder::writeInt(int64_t i, bool isUnsigned) {
        if (i >= -2048 && i < 2048) {
            uint8_t buf[2] = {(uint8_t)((i >> 8) & 0x0F),
                              (uint8_t)(i & 0xFF)};
            writeValue(value::kShortIntTag, buf, 2);
        } else {
            uint8_t buf[10];
            size_t size = PutIntOfLength(&buf[1], i);
            buf[0] = size - 1;
            if (isUnsigned)
                buf[0] |= 0x08;
            ++size;
            if (size & 1)
                buf[size++] = 0;  // pad to even size
            writeValue(value::kIntTag, buf, size);
        }
    }

    void encoder::writeDouble(double n) {
        if (isnan(n))
            throw "Can't write NaN";
        if (n == (int64_t)n) {
            return writeInt((int64_t)n);
        } else {
            littleEndianDouble swapped = n;
            uint8_t buf[2 + sizeof(swapped)];
            buf[0] = sizeof(swapped);
            buf[1] = 0;
            memcpy(&buf[2], &swapped, sizeof(swapped));
            writeValue(value::kFloatTag, buf, sizeof(buf));
        }
    }

    void encoder::writeFloat(float n) {
        if (isnan(n))
            throw "Can't write NaN";
        if (n == (int32_t)n) {
            return writeInt((int32_t)n);
        } else {
            littleEndianFloat swapped = n;
            uint8_t buf[2 + sizeof(swapped)];
            buf[0] = sizeof(swapped);
            buf[1] = 0;
            memcpy(&buf[2], &swapped, sizeof(swapped));
            writeValue(value::kFloatTag, buf, sizeof(buf));
        }
    }

    // used for strings and binary data
    void encoder::writeData(value::tags tag, slice s) {
        uint8_t buf[2 + kMaxVarintLen64];
        buf[0] = std::min(s.size, (size_t)0xF);
        if (s.size <= 1) {
            // Tiny data fits inline:
            if (s.size == 1)
                buf[1] = s[0];
            writeValue(tag, buf, 2, true);
        } else {
            // Large data doesn't:
            size_t bufLen = 1;
            if (s.size >= 0x0F)
                bufLen += PutUVarInt(&buf[1], s.size);
            writeValue(tag, buf, bufLen, false);
            _out << s;
        }
    }

    void encoder::writeString(std::string s) {writeData(value::kStringTag, slice(s));}
    void encoder::writeString(slice s)       {writeData(value::kStringTag, s);}
    void encoder::writeData(slice s)         {writeData(value::kBinaryTag, s);}

    encoder encoder::writeArrayOrDict(value::tags tag, uint32_t count) {
        uint8_t buf[2 + kMaxVarintLen32];
        uint32_t inlineCount = std::min(count, (uint32_t)0x0FFF);
        buf[0] = inlineCount >> 8;
        buf[1] = inlineCount & 0xFF;
        size_t bufLen = 2;
        if (count >= 0x0FFF) {
            bufLen += PutUVarInt(&buf[2], count);
            if (bufLen & 1)
                buf[bufLen++] = 0;
        }
        writeValue(tag, buf, bufLen, (count==0));          // can inline only if empty

        size_t offset = _out.length();
        size_t keyOffset = 0;
        size_t space = 2*count;
        if (tag == value::kDictTag) {
            keyOffset = offset;
            offset += 2*count;
            space *= 2;
        }
        _out.reserveSpace(space);

        return encoder(this, offset, keyOffset, count);
    }

    void encoder::writeKey(std::string s)   {writeKey(slice(s));}

    void encoder::writeKey(slice s) {
        if (!_blockedOnKey)
            throw _keyOffset>0 ? "need a value after a key" : "not a dictionary";
        _blockedOnKey = false;
        writeString(s);
    }

    void encoder::end() {
        if (_count > 0)
            throw "not all items were written";
    }

}
