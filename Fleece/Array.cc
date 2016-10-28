//
//  Array.cc
//  Fleece
//
//  Created by Jens Alfke on 5/12/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "Array.hh"
#include "SharedKeys.hh"
#include "Internal.hh"
#include "FleeceException.hh"
#include "varint.hh"
#include "PlatformCompat.hh"
#include <assert.h>
#include <iostream>


namespace fleece {
    using namespace internal;


#ifndef NDEBUG
    namespace internal {
        unsigned gTotalComparisons;
    }
#endif


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
            while (!WIDE && _usuallyFalse(v->isPointer()))
                v = derefPointer<true>(v);      // subsequent pointers must be wide
        }
        return v;
    }

    // Explicitly instantiate both needed versions:
    template const Value* Value::deref<false>(const Value *v);
    template const Value* Value::deref<true>(const Value *v);


#pragma mark - ARRAY:


    Array::impl::impl(const Value* v) noexcept {
        if (v == nullptr) {
            _first = nullptr;
            _wide = false;
            _count = 0;
            return;
        }
        
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
        throwIf(_count == 0, OutOfRange, "iterating past end of array");
        if (--_count == 0)
            return false;
        _first = _first->next(_wide);
        return true;
    }

    const Value* Array::impl::operator[] (unsigned index) const noexcept {
        if (index >= _count)
            return nullptr;
        if (_wide)
            return Value::deref<true> (offsetby(_first, kWide   * index));
        else
            return Value::deref<false>(offsetby(_first, kNarrow * index));
    }

    size_t Array::impl::indexOf(const Value *v) const noexcept {
        return ((size_t)v - (size_t)_first) / width(_wide);
    }



    uint32_t Array::count() const noexcept {
        return Array::impl(this)._count;
    }

    const Value* Array::get(uint32_t index) const noexcept {
        return impl(this)[index];
    }



    Array::iterator::iterator(const Array *a) noexcept
    :_a(a),
     _value(_a.firstValue())
    { }

    Array::iterator& Array::iterator::operator++() {
        _a.next();
        _value = _a.firstValue();
        return *this;
    }

    Array::iterator& Array::iterator::operator += (uint32_t n) {
        throwIf(n > _a._count, OutOfRange, "iterating past end of array");
        _a._count -= n;
        _a._first = offsetby(_a._first, width(_a._wide)*n);
        _value = _a.firstValue();
        return *this;
    }


#pragma mark - DICT:


    template <bool WIDE>
    struct dictImpl : public Array::impl {

        dictImpl(const Dict *d) noexcept
        :impl(d)
        { }

        const Value* get_unsorted(slice keyToFind) const noexcept {
            const Value *key = _first;
            for (uint32_t i = 0; i < _count; i++) {
                const Value *val = next(key);
                if (keyToFind.compare(keyBytes(key)) == 0)
                    return deref(val);
                key = next(val);
            }
            return nullptr;
        }

        inline const Value* get(slice keyToFind) const noexcept {
            auto key = (const Value*) ::bsearch(&keyToFind, _first, _count, 2*kWidth, &keyCmp);
            if (!key)
                return nullptr;
            return deref(next(key));
        }

        inline const Value* get(int keyToFind) const noexcept {
            auto key = (const Value*) ::bsearch((void*)(ssize_t)keyToFind,
                                                _first, _count, 2*kWidth,
                                                &numericKeyCmp);
            if (!key)
                return nullptr;
            return deref(next(key));
        }

        const bool lookupSharedKey(slice keyToFind, SharedKeys *sharedKeys, int &encoded) const noexcept {
            if (sharedKeys->encode(keyToFind, encoded))
                return true;
            // Key is not known to my SharedKeys; see if dict contains any unknown keys:
            if (_count == 0)
                return false;
            const Value *v = offsetby(_first, (_count-1)*2*kWidth);
            do {
                if (v->isInteger()) {
                    if (sharedKeys->isUnknownKey((int)v->asInt())) {
                        // Yup, try updating SharedKeys and re-encoding:
                        sharedKeys->refresh();
                        return sharedKeys->encode(keyToFind, encoded);
                    }
                    return false;
                }
            } while (--v >= _first);
            return false;
        }

        inline const Value* get(slice keyToFind, SharedKeys *sharedKeys) const noexcept {
            int encoded;
            if (sharedKeys && lookupSharedKey(keyToFind, sharedKeys, encoded))
                return get(encoded);
            return get(keyToFind);
        }

        const Value* get(Dict::key &keyToFind) const noexcept {
            auto sharedKeys = keyToFind._sharedKeys;
            if (sharedKeys) {
                // Look for a numeric key first:
                if (keyToFind._hasNumericKey)
                    return get(keyToFind._numericKey);
                // Key was not registered last we checked; see if dict contains any new keys:
                if (_count == 0)
                    return nullptr;
                if (lookupSharedKey(keyToFind._rawString, sharedKeys, keyToFind._numericKey)) {
                    keyToFind._hasNumericKey = true;
                    return get(keyToFind._numericKey);
                }
            }

            // Look up by string:
            const Value *key = findKeyByHint(keyToFind);
            if (!key) {
                const Value *end = offsetby(_first, _count*2*kWidth);
                if (!findKeyByPointer(keyToFind, _first, end, &key))
                    key = findKeyBySearch(keyToFind, _first, end);
            }
            return key ? deref(next(key)) : nullptr;
        }

#ifdef _MSC_VER
    #define log(FMT, PARAM, ...)
#else
    #define log(FMT, PARAM...) ({})
#endif

#if 0 // Set this to 1 to log innards of the methods below
    #undef log
    #ifdef _MSC_VER
        // Can't get this to compile
        //#define log(FMT, PARAM, ...) ({for (unsigned i_=0; i_<indent; i_++) std::cerr << "\t"; fprintf(stderr, FMT "\n", PARAM, __VA_ARGS__);})
    #else
        #define log(FMT, PARAM...) ({for (unsigned i_=0; i_<indent; i_++) std::cerr << "\t"; fprintf(stderr, FMT "\n", PARAM);})
    #endif
#endif

        size_t get(Dict::key keysToFind[], const Value* values[], size_t nKeys) noexcept {
            size_t nFound = 0;
            unsigned indent = 0;
            log("get(%zu keys; dict has %u)", nKeys, _count);
            nFound = find(keysToFind, values, 0, (unsigned)nKeys, 0, _count, indent+1).nFound;
            log("--> found %zu", nFound);
            // Note: There were two earlier implementations that can be found in older revisions.
            return nFound;
        }


        struct findResult {
            unsigned kMin, kMax;
            unsigned nFound;
        };


        // Finds the values for a sorted list of keys. Recursive, with a depth of log2(n).
        // [kf0..kf1) is the range in keysToFind[] to consider
        // [k0..k1) is the range in the dict's entry array to consider
        // indent is only used when debugging, to align the log output
        findResult find(Dict::key keysToFind[], const Value* values[],
                        unsigned kf0, unsigned kf1,
                        unsigned k0, unsigned k1,
                        unsigned indent) noexcept
        {
            log("find( %u--%u in %u--%u )", kf0, kf1-1, k0, k1-1);
            if (kf0 == kf1) {
                return {k0, k1, 0};
            }
            if (k0 == k1) {
                for (unsigned i = kf0; i < kf1; i++) {
                    values[i] = nullptr;
                    log("[#%u] = missing", i);
                }
                return {k0, k1, 0};
            }
            unsigned midf = (kf0 + kf1) / 2;
            unsigned midk = keysToFind[midf]._hint;
            if (midk < k0 || midk >= k1)
                midk = (k0 + k1 ) / 2;
            const Value *key = offsetby(_first, 2*kWidth*midk);
            int cmp = keysToFind[midf]._rawString.compare(keyBytes(key));
#ifndef NDEBUG
            ++gTotalComparisons;
#endif
            log("#%u '%s' ?vs? #%u '%s'",
                    midf, ((std::string)keysToFind[midf]._rawString).c_str(),
                    midk, ((std::string)keyBytes(key)).c_str());
            findResult left, right, result = {0, 0, 0};
            if (cmp == 0) {
                log("[#%u] = #%u", midf, midk);
                values[midf] = deref(next(key));
                keysToFind[midf]._hint = midk;
                left  = find(keysToFind, values, kf0,    midf, k0, midk, indent + 1);
                right = find(keysToFind, values, midf+1, kf1,  midk+1, k1, indent + 1);
                ++result.nFound;
            } else if (cmp < 0) {
                left  = find(keysToFind, values, kf0, midf+1, k0, midk, indent + 1);
                right = find(keysToFind, values, midf+1, kf1, left.kMax, k1, indent + 1);
            } else {
                right = find(keysToFind, values, midf, kf1, midk+1, k1, indent + 1);
                left  = find(keysToFind, values, kf0, midf, k0, right.kMin, indent + 1);
            }

            result.kMin = left.nFound ? left.kMin : (cmp==0 ? midk : right.kMin);
            result.kMax = right.nFound ? right.kMax : (cmp==0 ? midk : left.kMax);
            result.nFound += left.nFound + right.nFound;
            log("--> {%u--%u, found %u}", result.kMin, result.kMax-1, result.nFound);
            return result;
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
            return nullptr;
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
            *outKey = nullptr;
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
                return nullptr;

            // Found it! Cache dict index and encoded key as optimizations for next time:
            if (key->isPointer() && keyToFind._cachePointer)
                keyToFind._keyValue = deref(key);
            keyToFind._hint = (uint32_t)indexOf(key) / 2;
            return key;
        }

        static inline slice keyBytes(const Value *key) {
            return deref(key)->getStringBytes();
        }

        static inline const Value* next(const Value *v) {
            return v->next<WIDE>();
        }

        static inline const Value* deref(const Value *v) {
            return Value::deref<WIDE>(v);
        }

        static int keyCmp(const void* keyToFindP, const void* keyP) {
#ifndef NDEBUG
            ++gTotalComparisons;
#endif
            auto key = (const Value*)keyP;
            if (key->isInteger())
                return 1;
            else
                return ((slice*)keyToFindP)->compare(keyBytes(key));
        }

        static int numericKeyCmp(const void* keyToFindP, const void* keyP) {
#ifndef NDEBUG
            ++gTotalComparisons;
#endif
            auto key = (const Value*)keyP;
            if (key->isInteger())
                return (int)((ssize_t)keyToFindP - key->asInt());
            else
                return -1;
        }

        static constexpr size_t kWidth = (WIDE ? 4 : 2);
        static constexpr uint32_t kPtrMask = (WIDE ? 0x80000000 : 0x8000);
    };



    uint32_t Dict::count() const noexcept {
        return Array::impl(this)._count;
    }
    
    const Value* Dict::get_unsorted(slice keyToFind) const noexcept {
        if (isWideArray())
            return dictImpl<true>(this).get_unsorted(keyToFind);
        else
            return dictImpl<false>(this).get_unsorted(keyToFind);
    }

    const Value* Dict::get(slice keyToFind) const noexcept {
        if (isWideArray())
            return dictImpl<true>(this).get(keyToFind);
            else
                return dictImpl<false>(this).get(keyToFind);
                }
    
    const Value* Dict::get(slice keyToFind, SharedKeys *sk) const noexcept {
        if (isWideArray())
            return dictImpl<true>(this).get(keyToFind, sk);
        else
            return dictImpl<false>(this).get(keyToFind, sk);
    }

    const Value* Dict::get(int keyToFind) const noexcept {
        if (isWideArray())
            return dictImpl<true>(this).get(keyToFind);
        else
            return dictImpl<false>(this).get(keyToFind);
    }

    const Value* Dict::get(key &keyToFind) const noexcept {
        if (isWideArray())
            return dictImpl<true>(this).get(keyToFind);
        else
            return dictImpl<false>(this).get(keyToFind);
    }

    size_t Dict::get(key keys[], const Value* values[], size_t count) const noexcept {
        if (isWideArray())
            return dictImpl<true>(this).get(keys, values, count);
        else
            return dictImpl<false>(this).get(keys, values, count);
    }

    static int sortKeysCmp(const void *a, const void *b) {
        auto k1 = (Dict::key*)a, k2 = (Dict::key*)b;
        return k1->compare(*k2);

    }

    void Dict::sortKeys(key keys[], size_t count) noexcept {
        qsort(keys, count, sizeof(key), sortKeysCmp);
    }



    Dict::iterator::iterator(const Dict* d) noexcept
    :_a(d)
    {
        readKV();
    }

    Dict::iterator::iterator(const Dict* d, const SharedKeys *sk) noexcept
    :_a(d), _sharedKeys(sk)
    {
        readKV();
    }

    slice Dict::iterator::keyString() const noexcept {
        slice keyStr = _key->asString();
        if (!keyStr && _key->isInteger() && _sharedKeys)
            keyStr = _sharedKeys->decode((int)_key->asInt());
        return keyStr;
    }

    Dict::iterator& Dict::iterator::operator++() {
        throwIf(_a._count == 0, OutOfRange, "iterating past end of dict");
        --_a._count;
        _a._first = offsetby(_a._first, 2*width(_a._wide));
        readKV();
        return *this;
    }

    Dict::iterator& Dict::iterator::operator += (uint32_t n) {
        throwIf(n > _a._count, OutOfRange, "iterating past end of dict");
        _a._count -= n;
        _a._first = offsetby(_a._first, 2*width(_a._wide)*n);
        readKV();
        return *this;
    }

    void Dict::iterator::readKV() noexcept {
        if (_a._count) {
            _key   = deref(_a._first,                _a._wide);
            _value = deref(_a._first->next(_a._wide), _a._wide);
        } else {
            _key = _value = nullptr;
        }
    }


    Dict::key::key(slice rawString)
    :_rawString(rawString), _cachePointer(false)
    { }


    Dict::key::key(slice rawString, SharedKeys *sk, bool cachePointer)
    :_rawString(rawString), _sharedKeys(sk), _cachePointer(cachePointer)
    {
        int n;
        if (sk && sk->encode(rawString, n)) {
            _numericKey = (uint32_t)n;
            _hasNumericKey = true;
        }
    }

}
