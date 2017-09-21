//
//  MutableArray.cc
//  Fleece
//
//  Created by Jens Alfke on 9/19/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "MutableArray.hh"
#include "varint.hh"

namespace fleece {

    using namespace internal;


#pragma mark - FAT VALUE:

    namespace internal {

        static_assert(kFat == sizeof(MutableValue), "kFat is wrong");


        void MutableValue::set(Null) {
            set(kSpecialTag, kSpecialValueNull);
        }


        void MutableValue::set(bool b) {
            set(kSpecialTag, b ? kSpecialValueTrue : kSpecialValueFalse);
        }


        void MutableValue::set(int64_t i)   {set(i, (i < 2048 && i >= -2048), false);}
        void MutableValue::set(uint64_t i)  {set(i, (i < 2048),               true);}

        void MutableValue::set(uint64_t i, bool isSmall, bool isUnsigned) {
            if (isSmall) {
                set(kShortIntTag, (i >> 8) & 0x0F, i & 0xFF);
            } else {
                int size = (int)PutIntOfLength(&_byte[1], i, isUnsigned) - 1;
                if (isUnsigned)
                    size |= 0x08;
                set(kIntTag, size);
            }
        }


        void MutableValue::set(tags tag, slice s) {
            if (s.size < sizeof(MutableValue)) {
                set(tag, (int)s.size);
                memcpy(&_byte[1], s.buf, s.size);
            } else {
                assert(s.size < sizeof(MutableValue)); // TODO
            }
        }


        void MutableValue::set(const Value* v) {
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
                set(v); // Just store a pointer to the array/dict
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

    }


#pragma mark - MUTABLE ARRAY:


    MutableArray::MutableArray(const Array *a)
    :MutableArray(a->count())
    {
        auto dst = _items.begin();
        for (Array::iterator src(a); src; ++src, ++dst) {
            dst->copy(src.value());
        }
    }


    void MutableArray::resize(uint32_t newSize) {
        _items.resize(newSize);
    }


    void MutableArray::insert(uint32_t where, uint32_t n) {
        throwIf(where > count(), OutOfRange, "insert position is past end of array");
        _items.insert(_items.begin() + where,  n, MutableValue());
    }


    void MutableArray::remove(uint32_t where, uint32_t n) {
        throwIf(where + n > count(), OutOfRange, "remove range is past end of array");
        auto at = _items.begin() + where;
        _items.erase(at, at + n);
    }

}
