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

    bool Dict::isMagicParentKey(const Value *v) {
        return v->_byte[0] == uint8_t((kShortIntTag<<4) | 0x08)
            && v->_byte[1] == 0;
    }


#pragma mark - DICTIMPL CLASS:

    template <bool WIDE>
    struct dictImpl : public Array::impl {

        dictImpl(const Dict *d) noexcept
        :impl(d)
        { }

        bool givenNecessarySharedKeys(SharedKeys *sk) const {
            return sk || _count == 0 || deref(_first)->tag() == kStringTag
                || (Dict::isMagicParentKey(deref(_first))
                        && (_count == 1 || deref(offsetby(_first, 2*_width))->tag() == kStringTag))
                || gDisableNecessarySharedKeysCheck;
        }

        template <class KEY>
        const Value* finishGet(const Value *keyFound, KEY keyToFind) const noexcept {
            if (keyFound) {
                auto value = deref(next(keyFound));
                if (_usuallyFalse(value->isUndefined()))
                    value = nullptr;
                return value;
            } else {
                const Dict *parent = getParent();
                return parent ? parent->get(keyToFind) : nullptr;
            }
        }

        inline const Value* getUnshared(slice keyToFind) const noexcept {
            auto key = search(keyToFind, [](slice target, const Value *val) {
                countComparison();
                return compareKeys(target, val);
            });
            return finishGet(key, keyToFind);
        }

        inline const Value* get(int keyToFind) const noexcept {
            assert(keyToFind >= 0);
            auto key = search(keyToFind, [](int target, const Value *key) {
                countComparison();
                return compareKeys(target, key);
            });
            return finishGet(key, keyToFind);
        }

        inline const Value* get(slice keyToFind, SharedKeys *sharedKeys =nullptr) const noexcept {
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
            return finishGet(key, keyToFind);
        }

        bool hasParent() const {
            return _usuallyTrue(_count > 0) && _usuallyFalse(Dict::isMagicParentKey(deref(_first)));
        }

        const Dict* getParent() const {
            return hasParent() ? (const Dict*)deref(second()) : nullptr;
        }


        static int compareKeys(slice keyToFind, const Value *key) {
            if (_usuallyTrue(key->isInteger()))
                return 1;
            else
                return keyToFind.compare(keyBytes(key));
        }

        static int compareKeys(int keyToFind, const Value *key) {
            if (_usuallyTrue(key->tag() == kShortIntTag))
                return (int)(keyToFind - key->shortValue());
            else if (_usuallyFalse(key->tag() == kIntTag))
                return (int)(keyToFind - key->asInt());
            else
                return -1;
        }

        static int compareKeys(const Value *keyToFind, const Value *key) {
            if (keyToFind->tag() == kStringTag)
                return compareKeys(keyBytes(keyToFind), key);
            else
                return compareKeys((int)keyToFind->asInt(), key);
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
                        || (compareKeys(keyToFind._rawString, key) == 0)) {
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
            auto key = search(keyToFind._rawString, [](slice target, const Value *val) {
                return compareKeys(target, val);
            });
            if (!key)
                return nullptr;

            // Found it! Cache dict index and encoded key as optimizations for next time:
            if (key->isPointer() && keyToFind._cachePointer)
                keyToFind._keyValue = deref(key);
            keyToFind._hint = (uint32_t)indexOf(key) / 2;
            return key;
        }

        bool lookupSharedKey(slice keyToFind, SharedKeys *sharedKeys, int &encoded) const noexcept {
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
            return v->deref<WIDE>();
        }

        static constexpr size_t kWidth = (WIDE ? 4 : 2);
        static constexpr uint32_t kPtrMask = (WIDE ? 0x80000000 : 0x8000);
    };


    static int compareKeys(const Value *keyToFind, const Value *key, bool wide) {
        if (wide)
            return dictImpl<true>::compareKeys(keyToFind, key);
        else
            return dictImpl<true>::compareKeys(keyToFind, key);
    }


#pragma mark - DICT IMPLEMENTATION:


    uint32_t Dict::rawCount() const noexcept {
        if (_usuallyFalse(isMutable()))
            return heapDict()->count();
        return Array::impl(this)._count;
    }

    uint32_t Dict::count() const noexcept {
        if (_usuallyFalse(isMutable()))
            return heapDict()->count();
        Array::impl imp(this);
        if (_usuallyFalse(imp._count > 1 && isMagicParentKey(imp._first))) {
            // Dict has a parent; this makes counting much more expensive!
            uint32_t c = 0;
            for (iterator i(this); i; ++i)
                ++c;
            return c;
        } else {
            return imp._count;
        }
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

    MutableDict* Dict::asMutable() const {
        return isMutable() ? (MutableDict*)this : nullptr;
    }

    HeapDict* Dict::heapDict() const {
        return (HeapDict*)internal::HeapCollection::asHeapValue(this);
    }

    const Dict* Dict::getParent() const {
        if (isMutable())
            return heapDict()->source();
        else if (isWideArray())
            return dictImpl<true>(this).getParent();
        else
            return dictImpl<false>(this).getParent();
    }


    static constexpr Dict kEmptyDictInstance;
    const Dict* const Dict::kEmpty = &kEmptyDictInstance;


#pragma mark - DICT::ITERATOR:


    Dict::iterator::iterator(const Dict* d) noexcept
    :iterator(d, nullptr)
    { }

    Dict::iterator::iterator(const Dict* d, const SharedKeys *sk) noexcept
    :_a(d), _sharedKeys(sk)
    {
        readKV();
        if (_usuallyFalse(_key && Dict::isMagicParentKey(_key))) {
            _parent.reset( new iterator(_value->asDict()) );
            ++(*this);
        }
    }

    Dict::iterator::iterator(const Dict* d, bool) noexcept
    :_a(d)
    {
        readKV();
        // skips the parent check, so it will iterate the raw contents
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
        do {
            if (_keyCmp >= 0)
                ++(*_parent);
            if (_keyCmp <= 0) {
                throwIf(_a._count == 0, OutOfRange, "iterating past end of dict");
                --_a._count;
                _a._first = offsetby(_a._first, 2*_a._width);
            }
            readKV();
        } while (_usuallyFalse(_parent && _value && _value->isUndefined()));      // skip deletion tombstones
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

        if (_usuallyFalse(_parent != nullptr)) {
            auto parentKey = _parent->key();
            if (_usuallyFalse(!_key))
                _keyCmp = parentKey ? 1 : 0;
            else if (_usuallyFalse(!parentKey))
                _keyCmp = _key ? -1 : 0;
            else
                _keyCmp = compareKeys(_key, parentKey, (_a._width > kNarrow));
            if (_keyCmp > 0) {
                _key = parentKey;
                _value = _parent->value();
            }
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
