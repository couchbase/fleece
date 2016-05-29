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
            while (!WIDE && __builtin_expect(v->isPointer(), false))
                v = derefPointer<true>(v);      // subsequent pointers must be wide
        }
        return v;
    }

    // Explicitly instantiate both needed versions:
    template const Value* Value::deref<false>(const Value *v);
    template const Value* Value::deref<true>(const Value *v);


#pragma mark - ARRAY:


    Array::impl::impl(const Value* v) {
        _first = (const Value*)(&v->_byte[2]);
        _wide = v->isWideArray();
        _count = v->shortValue() & 0x07FF;
        if (_count == kLongArrayCount) {
            // Long count is stored as a varint:
            uint32_t extraCount;
            size_t countSize = GetUVarInt32(slice(_first, 10), &extraCount);
            assert(countSize > 0);
            _count += extraCount;
            _first = offsetby(_first, countSize + (countSize & 1));
        }
    }

    bool Array::impl::next() {
        if (_count == 0)
            throw "iterating past end of array";
        if (--_count == 0)
            return false;
        _first = _first->next(_wide);
        return true;
    }

    const Value* Array::impl::operator[] (unsigned index) const {
        if (index >= _count)
            return NULL;
        if (_wide)
            return Value::deref<true> (offsetby(_first, kWide   * index));
        else
            return Value::deref<false>(offsetby(_first, kNarrow * index));
    }

    size_t Array::impl::indexOf(const Value *v) const {
        return ((size_t)v - (size_t)_first) / width(_wide);
    }



    uint32_t Array::count() const {
        return Array::impl(this)._count;
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
        if (n > _a._count)
            throw "iterating past end of array";
        _a._count -= n;
        _a._first = offsetby(_a._first, width(_a._wide)*n);
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
            const Value *key = _first;
            for (uint32_t i = 0; i < _count; i++) {
                const Value *val = next(key);
                if (keyToFind.compare(deref(key)->asString()) == 0)
                    return deref(val);
                key = next(val);
            }
            return NULL;
        }

        inline const Value* get(slice keyToFind) const {
            auto key = (const Value*) ::bsearch(&keyToFind, _first, _count, 2*kWidth, &keyCmp);
            if (!key)
                return NULL;
            return deref(next(key));
        }

        const Value* get(Dict::key &keyToFind) const {
            const Value *key = findKeyByHint(keyToFind);
            if (!key) {
                const Value *end = offsetby(_first, _count*2*kWidth);
                if (!findKeyByPointer(keyToFind, _first, end, &key))
                    key = findKeyBySearch(keyToFind, _first, end);
            }
            return key ? deref(next(key)) : NULL;
        }

        size_t get(Dict::key keysToFind[], const Value* values[], size_t nKeys) {
            size_t nFound = 0;
            const Value *start = _first;
            const Value *end = offsetby(_first, _count*2*kWidth);
            for (size_t i = 0; i < nKeys; i++) {
                Dict::key &keyToFind = keysToFind[i];
                const Value *key = findKeyByHint(keyToFind);
                if (!key && !findKeyByPointer(keyToFind, start, end, &key))
                    key = findKeyBySearch(keyToFind, start, end);

                if (key) {
                    auto value = next(key);
                    values[i] = deref(value);
                    ++nFound;
                    start = next(value);        // Start next search after the found key/value
                } else {
                    values[i] = NULL;
                }
            }
            return nFound;
        }
        
    private:

        const Value* findKeyByHint(Dict::key &keyToFind) const {
            if (keyToFind._hint < _count) {
                const Value *key  = offsetby(_first, keyToFind._hint * 2 * kWidth);
                if ((keyToFind._keyValue && key->isPointer() && deref(key) == keyToFind._keyValue)
                        || (keyCmp(&keyToFind._rawString, key) == 0)) {
                    return key;
                }
            }
            return NULL;
        }

        // Find a key in a dictionary by comparing the cached pointer with the pointers in the
        // dict. If this isn't possible, returns false.
        bool findKeyByPointer(Dict::key &keyToFind, const Value *start, const Value *end,
                              const Value **outKey) const
        {
            // Check whether there's a cached key pointer, and the key would be a pointer:
            if (!keyToFind._keyValue || (keyToFind._rawString.size < kWidth))
                return false;
            // Check whether the key is in pointer range of this dict:
            const Value *key = start;
            size_t maxOffset = (WIDE ?0xFFFFFFFF : 0xFFFF);
            auto offset = (size_t)((uint8_t*)key - (uint8_t*)keyToFind._keyValue);
            auto offsetAtEnd = (size_t)((uint8_t*)end - kWidth - (uint8_t*)keyToFind._keyValue);
            if (offset > maxOffset || offsetAtEnd > maxOffset)
                return false;
            // OK, key Value is in range so we can use it here, for a linear scan.
            // Raw integer key we're looking for (in native byte order):
            auto rawKeyToFind = (uint32_t)((offset >> 1) | kPtrMask);
            while (key < end) {
                if (WIDE ? (_dec32(*(uint32_t*)key) == rawKeyToFind)
                    : (_dec16(*(uint16_t*)key) == (uint16_t)rawKeyToFind)) {
                    // Found it! Cache the dict index as a hint for next time:
                    keyToFind._hint = (uint32_t)indexOf(key) / 2;
                    *outKey = key;
                    return true;
                }
                rawKeyToFind += kWidth;      // offset to string increases as key advances
                key = next(next(key));
            }
            // Definitively not found
            *outKey = NULL;
            return true;
        }

        // Finds a key in a dictionary via binary search of the UTF-8 key strings.
        const Value* findKeyBySearch(Dict::key &keyToFind,
                                     const Value *start, const Value *end) const
        {
            auto key = (const Value*) ::bsearch(&keyToFind._rawString, start,
                                                ((ptrdiff_t)end - (ptrdiff_t)start) / (2*kWidth),
                                                2*kWidth, &keyCmp);
            if (!key)
                return NULL;

            // Found it! Cache dict index and encoded key as optimizations for next time:
            if (key->isPointer() && keyToFind._cachePointer)
                keyToFind._keyValue = deref(key);
            keyToFind._hint = (uint32_t)indexOf(key) / 2;
            return key;
        }

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
        return Array::impl(this)._count;
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

    size_t Dict::get(key keys[], const Value* values[], size_t count) const {
        if (isWideArray())
            return dictImpl<true>(this).get(keys, values, count);
        else
            return dictImpl<false>(this).get(keys, values, count);
    }

    static int sortKeysCmp(const void *a, const void *b) {
        auto k1 = (Dict::key*)a, k2 = (Dict::key*)b;
        return k1->compare(*k2);

    }

    void Dict::sortKeys(key keys[], size_t count) {
        qsort(keys, count, sizeof(key), sortKeysCmp);
    }



    Dict::iterator::iterator(const Dict* d)
    :_a(d)
    {
        readKV();
    }

    Dict::iterator& Dict::iterator::operator++() {
        if (_a._count == 0)
            throw "iterating past end of dict";
        --_a._count;
        _a._first = offsetby(_a._first, 2*width(_a._wide));
        readKV();
        return *this;
    }

    Dict::iterator& Dict::iterator::operator += (uint32_t n) {
        if (n > _a._count)
            throw "iterating past end of dict";
        _a._count -= n;
        _a._first = offsetby(_a._first, 2*width(_a._wide)*n);
        readKV();
        return *this;
    }

    void Dict::iterator::readKV() {
        if (_a._count) {
            _key   = deref(_a._first,                _a._wide);
            _value = deref(_a._first->next(_a._wide), _a._wide);
        } else {
            _key = _value = NULL;
        }
    }

}
