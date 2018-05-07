//
// Dict.cc
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
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

#include "Dict.hh"
#include "MutableDict.hh"
#include "SharedKeys.hh"
#include "Internal.hh"
#include "PlatformCompat.hh"
#include <atomic>
#include <string>


namespace fleece {
    using namespace internal;


#ifndef NDEBUG
    namespace internal {
        std::atomic<unsigned> gTotalComparisons;
        bool gDisableNecessarySharedKeysCheck = false;
    }
    static inline void countComparison() {++gTotalComparisons;}
#else
    static inline void countComparison() { }
    static const bool gDisableNecessarySharedKeysCheck = false;
#endif


#pragma mark - DICTIMPL CLASS:

    template <bool WIDE>
    struct dictImpl : public Array::impl {

        dictImpl(const Dict *d) noexcept
        :impl(d)
        { }

        bool givenNecessarySharedKeys(SharedKeys *sk) const {
            return sk || (_count == 0 || deref(_first)->tag() == kStringTag)
                || gDisableNecessarySharedKeysCheck;
        }

        const Value* get_unsorted(slice keyToFind) const noexcept {
            const Value *key = _first;
            for (uint32_t i = 0; i < _count; i++) {
                const Value *val = next(key);
                if (_usuallyFalse(keyToFind.compare(keyBytes(key)) == 0))
                    return deref(val);
                key = next(val);
            }
            return nullptr;
        }

        inline const Value* getUnshared(slice keyToFind) const noexcept {
            auto key = search(&keyToFind, [](const slice *target, const Value *val) {
                return keyCmp(target, val);
            });
            if (!key)
                return nullptr;
            return deref(next(key));
        }

        inline const Value* get(slice keyToFind) const noexcept {
            assert(givenNecessarySharedKeys(nullptr));
            return getUnshared(keyToFind);
        }

        inline const Value* get(int keyToFind) const noexcept {
            auto key = search(keyToFind, [](int target, const Value *key) {
                countComparison();
                if (_usuallyTrue(key->tag() == kShortIntTag))
                    return (int)(target - key->shortValue());
                else if (_usuallyFalse(key->tag() == kIntTag))
                    return (int)(target - key->asInt());
                else
                    return -1;
            });
            if (!key)
                return nullptr;
            return deref(next(key));
        }

        inline const Value* get(slice keyToFind, SharedKeys *sharedKeys) const noexcept {
            assert(givenNecessarySharedKeys(sharedKeys));
            int encoded;
            if (sharedKeys && lookupSharedKey(keyToFind, sharedKeys, encoded))
                return get(encoded);
            return getUnshared(keyToFind);
        }

        const Value* get(Dict::key &keyToFind) const noexcept {
            auto sharedKeys = keyToFind._sharedKeys;
            assert(givenNecessarySharedKeys(sharedKeys));
            if (_usuallyTrue(sharedKeys != nullptr)) {
                // Look for a numeric key first:
                if (_usuallyTrue(keyToFind._hasNumericKey))
                    return get(keyToFind._numericKey);
                // Key was not registered last we checked; see if dict contains any new keys:
                if (_usuallyFalse(_count == 0))
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
                    key = findKeyBySearch(keyToFind);
            }
            return key ? deref(next(key)) : nullptr;
        }

        size_t get(Dict::key keysToFind[], const Value* values[], size_t nKeys) noexcept {
            size_t nFound = 0;
            for (size_t i = 0; i < nKeys; ++i) {
                auto value = get(keysToFind[i]);
                values[i] = value;
                if (value)
                    ++nFound;
            }
            return nFound;
        }


    private:

        // typical binary search function; returns pointer to the key it finds
        template <class T, class CMP>
        inline const Value* search(T target, CMP comparator) const {
            const Value *begin = _first;
            size_t n = _count;
            while (n > 0) {
                size_t mid = n >> 1;
                const Value *midVal = offsetby(begin, mid * 2*kWidth);
                int cmp = comparator(target, midVal);
                if (_usuallyFalse(cmp == 0))
                    return midVal;
                else if (cmp < 0)
                    n = mid;
                else {
                    begin = offsetby(midVal, 2*kWidth);
                    n -= mid + 1;
                }
            }
            return nullptr;
        }

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
        const Value* findKeyBySearch(Dict::key &keyToFind) const {
            auto key = search(&keyToFind._rawString, [](const slice *target, const Value *val) {
                return keyCmp(target, val);
            });
            if (!key)
                return nullptr;

            // Found it! Cache dict index and encoded key as optimizations for next time:
            if (key->isPointer() && keyToFind._cachePointer)
                keyToFind._keyValue = deref(key);
            keyToFind._hint = (uint32_t)indexOf(key) / 2;
            return key;
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

        static inline slice keyBytes(const Value *key) {
            return deref(key)->getStringBytes();
        }

        static inline const Value* next(const Value *v) {
            return v->next<WIDE>();
        }

        static inline const Value* deref(const Value *v) {
            return Value::deref<WIDE>(v);
        }

        static int keyCmp(const slice *keyToFind, const Value *key) {
            countComparison();
            if (key->isInteger())
                return 1;
            else
                return keyToFind->compare(keyBytes(key));
        }

        static constexpr size_t kWidth = (WIDE ? 4 : 2);
        static constexpr uint32_t kPtrMask = (WIDE ? 0x80000000 : 0x8000);
    };


#pragma mark - DICT IMPLEMENTATION:


    uint32_t Dict::count() const noexcept {
        if (_usuallyFalse(isMutable()))
            return heapDict()->count();
        return Array::impl(this)._count;
    }

    const Value* Dict::get_unsorted(slice keyToFind) const noexcept {
        if (_usuallyFalse(isMutable()))
            return heapDict()->get(keyToFind);
        if (isWideArray())
            return dictImpl<true>(this).get_unsorted(keyToFind);
        else
            return dictImpl<false>(this).get_unsorted(keyToFind);
    }

    const Value* Dict::get(slice keyToFind) const noexcept {
        if (_usuallyFalse(isMutable()))
            return heapDict()->get(keyToFind);
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


    MutableDict* Dict::asMutable() const {
        return isMutable() ? (MutableDict*)this : nullptr;
    }

    HeapDict* Dict::heapDict() const {
        return (HeapDict*)internal::HeapCollection::asHeapValue(this);
    }


    static constexpr Dict kEmptyDictInstance;
    const Dict* const Dict::kEmpty = &kEmptyDictInstance;


#pragma mark - DICT::ITERATOR:


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
        if (!keyStr && _key->isInteger()) {
            assert(_sharedKeys || gDisableNecessarySharedKeysCheck);
            if (_sharedKeys)
                keyStr = _sharedKeys->decode((int)_key->asInt());
        }
        return keyStr;
    }

    Dict::iterator& Dict::iterator::operator++() {
        throwIf(_a._count == 0, OutOfRange, "iterating past end of dict");
        --_a._count;
        _a._first = offsetby(_a._first, 2*_a._width);
        readKV();
        return *this;
    }

    Dict::iterator& Dict::iterator::operator += (uint32_t n) {
        throwIf(n > _a._count, OutOfRange, "iterating past end of dict");
        _a._count -= n;
        _a._first = offsetby(_a._first, 2*_a._width*n);
        readKV();
        return *this;
    }

    void Dict::iterator::readKV() noexcept {
        if (_usuallyTrue(_a._count)) {
            _key   = _a.deref(_a._first);
            _value = _a.deref(_a.second());
        } else {
            _key = _value = nullptr;
        }
    }


#pragma mark - DICT::KEY:


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
