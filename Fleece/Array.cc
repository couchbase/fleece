//
//  Array.cc
//  Fleece
//
//  Created by Jens Alfke on 5/12/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "Array.hh"
#include "Internal.hh"
#include "varint.hh"
#include <assert.h>


namespace fleece {
    using namespace internal;


    const Value* Value::deref(const Value *v, bool wide) {
        while (v->isPointer()) {
            v = derefPointer(v, wide);
            wide = true;                        // subsequent pointers must be wide
        }
        return v;
    }

    template <bool WIDE>
    const Value* Value::deref(const Value *v) {
        if (v->isPointer()) {
            v = derefPointer<WIDE>(v);
            while (v->isPointer())
                v = derefPointer<true>(v);      // subsequent pointers must be wide
        }
        return v;
    }

    // Explicitly instantiate both needed versions:
    template const Value* Value::deref<false>(const Value *v);
    template const Value* Value::deref<true>(const Value *v);


#pragma mark - ARRAY:


    Array::impl::impl(const Value* v) {
        first = (const Value*)(&v->_byte[2]);
        wide = v->isWideArray();
        count = v->shortValue() & 0x07FF;
        if (count == kLongArrayCount) {
            // Long count is stored as a varint:
            uint32_t extraCount;
            size_t countSize = GetUVarInt32(slice(first, 10), &extraCount);
            assert(countSize > 0);
            count += extraCount;
            first = offsetby(first, countSize + (countSize & 1));
        }
    }

    bool Array::impl::next() {
        if (count == 0)
            throw "iterating past end of array";
        if (--count == 0)
            return false;
        first = first->next(wide);
        return true;
    }

    const Value* Array::impl::operator[] (unsigned index) const {
        if (index >= count)
            return NULL;
        if (wide)
            return Value::deref<true> (offsetby(first, kWide   * index));
        else
            return Value::deref<false>(offsetby(first, kNarrow * index));
    }

    size_t Array::impl::indexOf(const Value *v) const {
        return ((size_t)v - (size_t)first) / width(wide);
    }



    uint32_t Array::count() const {
        return Array::impl(this).count;
    }

    const Value* Array::get(uint32_t index) const {
        return impl(this)[index];
    }



    Array::iterator::iterator(const Array *a)
    :_a(a),
     _value(_a.firstValue())
    { }

    Array::iterator& Array::iterator::operator++() {
        _a.next();
        _value = _a.firstValue();
        return *this;
    }

    Array::iterator& Array::iterator::operator += (uint32_t n) {
        if (n > _a.count)
            throw "iterating past end of array";
        _a.count -= n;
        _a.first = offsetby(_a.first, width(_a.wide)*n);
        _value = _a.firstValue();
        return *this;
    }


#pragma mark - DICT:


    template <bool WIDE>
    struct dictImpl : public Array::impl {

        dictImpl(const Dict *d)
        :impl(d)
        { }

        const Value* get_unsorted(slice keyToFind) const {
            const Value *key = first;
            for (uint32_t i = 0; i < count; i++) {
                const Value *val = next(key);
                if (keyToFind.compare(deref(key)->asString()) == 0)
                    return deref(val);
                key = next(val);
            }
            return NULL;
        }

        inline const Value* get(slice keyToFind) const {
            auto key = (const Value*) ::bsearch(&keyToFind, first, count, 2*kWidth, &keyCmp);
            if (!key)
                return NULL;
            return deref(next(key));
        }

        const Value* get(Dict::key &keyToFind) const {
            const Value *start = first;

            // Use the index hint to possibly find the key in one probe:
            if (keyToFind._hint < count) {
                const Value *key  = offsetby(start, keyToFind._hint * 2 * kWidth);
                if ((keyToFind._keyValue && key->isPointer() && deref(key) == keyToFind._keyValue)
                        || (keyCmp(&keyToFind._rawString, key) == 0)) {
                    return deref(next(key));
                }
                // If the hint failed, look it up the slow way...
            }

            // Check whether there is a cached key and, if so, whether it would be used in this dict:
            if (keyToFind._keyValue && (keyToFind._rawString.size >= kWidth)) {
                const Value *key = start;
                const Value *end = offsetby(key, count*2*kWidth);
                size_t maxOffset = (WIDE ?0xFFFFFFFF : 0xFFFF);
                auto offset = (size_t)((uint8_t*)key - (uint8_t*)keyToFind._keyValue);
                auto offsetAtEnd = (size_t)((uint8_t*)end - kWidth - (uint8_t*)keyToFind._keyValue);
                if (offset <= maxOffset && offsetAtEnd <= maxOffset) {
                    // OK, key Value is in range so we can use it here, for a linear scan.
                    // Raw integer key we're looking for (in native byte order):
                    auto rawKeyToFind = (uint32_t)((offset >> 1) | kPtrMask);
                    while (key < end) {
                        const Value *val = next(key);
                        if (WIDE ? (_dec32(*(uint32_t*)key) == rawKeyToFind)
                                 : (_dec16(*(uint16_t*)key) == (uint16_t)rawKeyToFind)) {
                            // Found it! Cache the dict index as a hint for next time:
                            keyToFind._hint = (uint32_t)indexOf(key) / 2;
                            return deref(val);
                        }
                        rawKeyToFind += kWidth;      // offset to string increases as key advances
                        key = next(val);
                    }
                    return NULL;
                }
            }

            // Can't use the encoded key, so fall back to binary search by string bytes:
            auto key = (const Value*) ::bsearch(&keyToFind._rawString, start, count,
                                                2*kWidth, &keyCmp);
            if (!key)
                return NULL;

            // Found it! Cache dict index and encoded key as optimizations for next time:
            if (key->isPointer())
                keyToFind._keyValue = deref(key);
            keyToFind._hint = (uint32_t)indexOf(key) / 2;
            return deref(next(key));
        }

    private:
        static inline const Value* next(const Value *v) {
            return v->next<WIDE>();
        }

        static inline const Value* deref(const Value *v) {
            return Value::deref<WIDE>(v);
        }

        static int keyCmp(const void* keyToFindP, const void* keyP) {
            const Value *key = deref((const Value*)keyP);
            return ((slice*)keyToFindP)->compare(key->asString());
        }

        static const size_t kWidth = (WIDE ? 4 : 2);
        static const uint32_t kPtrMask = (WIDE ? 0x80000000 : 0x8000);
    };



    uint32_t Dict::count() const {
        return Array::impl(this).count;
    }
    
    const Value* Dict::get_unsorted(slice keyToFind) const {
        if (isWideArray())
            return dictImpl<true>(this).get_unsorted(keyToFind);
        else
            return dictImpl<false>(this).get_unsorted(keyToFind);
    }

    const Value* Dict::get(slice keyToFind) const {
        if (isWideArray())
            return dictImpl<true>(this).get(keyToFind);
        else
            return dictImpl<false>(this).get(keyToFind);
    }

    const Value* Dict::get(key &keyToFind) const {
        if (isWideArray())
            return dictImpl<true>(this).get(keyToFind);
        else
            return dictImpl<false>(this).get(keyToFind);
    }



    Dict::iterator::iterator(const Dict* d)
    :_a(d)
    {
        readKV();
    }

    Dict::iterator& Dict::iterator::operator++() {
        if (_a.count == 0)
            throw "iterating past end of dict";
        --_a.count;
        _a.first = offsetby(_a.first, 2*width(_a.wide));
        readKV();
        return *this;
    }

    Dict::iterator& Dict::iterator::operator += (uint32_t n) {
        if (n > _a.count)
            throw "iterating past end of dict";
        _a.count -= n;
        _a.first = offsetby(_a.first, 2*width(_a.wide)*n);
        readKV();
        return *this;
    }

    void Dict::iterator::readKV() {
        if (_a.count) {
            _key   = deref(_a.first,                _a.wide);
            _value = deref(_a.first->next(_a.wide), _a.wide);
        } else {
            _key = _value = NULL;
        }
    }

}
