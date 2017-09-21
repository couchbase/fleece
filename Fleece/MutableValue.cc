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

namespace fleece { namespace internal {

    static_assert(kFat == sizeof(MutableValue), "kFat is wrong");


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


    void MutableValue::set(tags tag, slice s) {
        if (s.size <= kMaxInlineValueSize) {
            setHeader(tag, (int)s.size);
            memcpy(&_byte[1], s.buf, s.size);
        } else {
            assert(s.size <= kMaxInlineValueSize); // TODO
        }
    }


    void MutableValue::_set(const Value* v) {
        throwIf(v == nullptr, InvalidData, "Can't set an array element to nullptr");
        // Store a (fat) pointer:
        int64_t n = _enc64((int64_t)v >> 1);
        memcpy(&_byte[0], &n, sizeof(n));
        _byte[0] |= 0x80;
    }


    void MutableValue::copy(const Value* v) {
        throwIf(v == nullptr, InvalidData, "Can't set an array element to nullptr");
        auto t = v->type();
        if (t == kArray || t == kDict) {
            _set(v); // Just store a pointer to the array/dict
        } else {
            size_t size = v->dataSize();
            assert(size <= sizeof(*this));
            memcpy(this, v, size);
        }
    }


    MutableArray* MutableValue::makeArrayMutable() {
        const Value *val = deref();
        switch (val->tag()) {
            case kArrayTag: {
                auto ma = new MutableArray((const Array*)val);
                set(ma);
                return ma;
            }
            case kSpecialTag:
                return val->asMutableArray();
            default:
                return nullptr;
        }
    }


    MutableDict* MutableValue::makeDictMutable() {
        const Value *val = deref();
        switch (val->tag()) {
            case kDictTag: {
                auto ma = new MutableDict((const Dict*)val);
                set(ma);
                return ma;
            }
            case kSpecialTag:
                return val->asMutableDict();
            default:
                return nullptr;
        }
    }

} }
