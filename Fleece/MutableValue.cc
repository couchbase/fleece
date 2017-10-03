//
//  MutableValue.cc
//  Fleece
//
//  Created by Jens Alfke on 9/21/17.
//Copyright © 2017 Couchbase. All rights reserved.
//

#include "MutableValue.hh"
#include "MutableArray.hh"
#include "MutableDict.hh"
#include "varint.hh"
#include <algorithm>

namespace fleece { namespace internal {
    using namespace std;


    static_assert(kFat == sizeof(MutableValue), "kFat is wrong");


    MutableValue::MutableValue(const MutableValue &mv)
    :_exists(true)
    ,_changed(false)
    ,_malloced(false)
    {
        if (mv._malloced)
            copy(&mv);
        else
            memcpy(&_byte, &mv._byte, kMaxInlineValueSize);
    }


    void MutableValue::reset() {
        if (_malloced) {
            free( (void*)deref() );
            _malloced = false;
        }
    }


    void MutableValue::set(Null) {
        setHeader(kSpecialTag, kSpecialValueNull);
    }


    void MutableValue::set(bool b) {
        setHeader(kSpecialTag, b ? kSpecialValueTrue : kSpecialValueFalse);
    }


    void MutableValue::set(int64_t i)   {set(i, (i < 2048 && i >= -2048), false);}
    void MutableValue::set(uint64_t i)  {set(i, (i < 2048),               true);}

    void MutableValue::set(uint64_t i, bool isSmall, bool isUnsigned) {
        if (isSmall) {
            setHeader(kShortIntTag, (i >> 8) & 0x0F, i & 0xFF);
        } else {
            int size = (int)PutIntOfLength(&_byte[1], i, isUnsigned) - 1;
            if (isUnsigned)
                size |= 0x08;
            setHeader(kIntTag, size);
        }
    }


    void MutableValue::_set(tags tag, slice s) {
        if (s.size <= kMaxInlineValueSize - 1) {
            // Short strings can go inline:
            setHeader(tag, (int)s.size);
            memcpy(&_byte[1], s.buf, s.size);
        } else {
            // Allocate a string Value on the heap. (Adapted from Encoder::writeData)
            auto buf = (uint8_t*)malloc(2 + kMaxVarintLen32 + s.size);
            auto strVal = new (buf) Value (kStringTag, (uint8_t)min(s.size, (size_t)0xF));
            size_t bufLen = 1;
            if (s.size >= 0x0F)
                bufLen += PutUVarInt(&buf[1], s.size);
            memcpy(&buf[bufLen], s.buf, s.size);
            reset();
            _setPointer(strVal);
            _malloced = _changed = true;
        }
    }


    // Setter subroutine that stores a (fat) pointer. Does not set _changed.
    void MutableValue::_setPointer(const Value* v) {
        throwIf(v == nullptr, InvalidData, "Can't set an array element to nullptr");
        int64_t n = _enc64((int64_t)v >> 1);
        memcpy(&_byte[0], &n, sizeof(n));
        _byte[0] |= 0x80;
        _exists = true;
    }


    void MutableValue::set(const Value* v) {
        reset();
        _changed = true;
        auto t = v->type();
        if (t != kArray && t != kDict) {
            auto size = v->dataSize();
            if (size <= kNarrow) {
                // Tiny value can be copied inline
                memcpy(&_byte[0], v, size);
                return;
            }
        }
        // Otherwise store a pointer to it
        _setPointer(v);
    }


    void MutableValue::copy(const Value* v) {
        reset();
        auto t = v->type();
        if (t != kArray && t != kDict) {
            size_t size = v->dataSize();
            if (size <= kMaxInlineValueSize) {
                // Value fits inline
                memcpy(&_byte[0], v, size);
            } else {
                // Value is too large to fit, so allocate a new heap block for it:
                v = (const Value*) slice(v, size).copy().buf;
                _malloced = true;
            }
        }
        _setPointer(v);
    }


    const Value* MutableValue::makeMutable(valueType ifType) {
        const Value *val = deref();
        if (val->type() != ifType)
            return nullptr;                                 // wrong type
        if (val->tag() == kSpecialTag)
            return val;                                     // already mutable

        const Value *newMutableResult;
        if (ifType == kArray)
            newMutableResult = new MutableArray((const Array*)val);
        else
            newMutableResult = new MutableDict((const Dict*)val);
        set(newMutableResult);
        _malloced = true;
        return newMutableResult;
    }

} }
