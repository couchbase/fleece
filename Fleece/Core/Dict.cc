//
// Dict.cc
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "Dict.hh"
#include "MutableDict.hh"
#include "SharedKeys.hh"
#include "Doc.hh"
#include "Internal.hh"
#include "fleece/PlatformCompat.hh"
#include <atomic>
#include <string>
#include "betterassert.hh"


namespace fleece { namespace impl {
    using namespace internal;


#ifndef NDEBUG
    namespace internal {
        std::atomic<unsigned> gTotalComparisons;
        bool gDisableNecessarySharedKeysCheck = false;
    }
    static inline void countComparison() {++gTotalComparisons;}
#else
    static inline void countComparison() { }
#endif

    bool Dict::isMagicParentKey(const Value *v) {
        return v->_byte[0] == uint8_t((kShortIntTag<<4) | 0x08)
            && v->_byte[1] == 0;
    }


#pragma mark - DICTIMPL CLASS:


    template <bool WIDE>
    struct dictImpl : public Array::impl {

        __hot
        dictImpl(const Dict *d) noexcept
        :impl(d)
        { }

        SharedKeys* findSharedKeys() const {
            return Doc::sharedKeys(_first);
        }

        bool usesSharedKeys() const {
            // Check if the first key is an int (the second, if the 1st is a parent ptr)
            return _count > 0 && _first->tag() == kShortIntTag
                && !(Dict::isMagicParentKey(_first)
                     && (_count == 1 || offsetby(_first, 2*_width)->tag() != kShortIntTag));
        }

        template <class KEY>
        __hot
        const Value* finishGet(const Value *keyFound, KEY &keyToFind, bool returnUndefined) const noexcept {
            if (keyFound) {
                auto value = deref(next(keyFound));
                if (!returnUndefined && _usuallyFalse(value->isUndefined()))
                    value = nullptr;
                return value;
            } else {
                const Dict *parent = getParent();
                return parent ? parent->get(keyToFind, returnUndefined) : nullptr;
            }
        }

        __hot
        inline const Value* getUnshared(slice keyToFind, bool returnUndefined) const noexcept {
            auto key = search(keyToFind, [](slice target, const Value *val) {
                countComparison();
                return compareKeys(target, val);
            });
            return finishGet(key, keyToFind, returnUndefined);
        }

        __hot
        inline const Value* search(int keyToFind) const noexcept {
            return search(keyToFind, [](int target, const Value *key) {
                countComparison();
                return compareKeys(target, key);
            });
        }

        __hot
        inline const Value* get(int keyToFind, bool returnUndefined= false) const noexcept {
            assert_precondition(keyToFind >= 0);
            return finishGet(search(keyToFind), keyToFind, returnUndefined);
        }

        __hot
        inline const Value* get(slice keyToFind, SharedKeys *sharedKeys =nullptr, bool returnUndefined= false) const noexcept {
            if (!sharedKeys && usesSharedKeys()) {
                sharedKeys = findSharedKeys();
                assert_precondition(sharedKeys || gDisableNecessarySharedKeysCheck);
            }
            int encoded;
            if (sharedKeys && lookupSharedKey(keyToFind, sharedKeys, encoded))
                return get(encoded, returnUndefined);
            return getUnshared(keyToFind, returnUndefined);
        }

        __hot
        const Value* get(Dict::key &keyToFind, bool returnUndefined= false) const noexcept {
            auto sharedKeys = keyToFind._sharedKeys;
            if (!sharedKeys && usesSharedKeys()) {
                sharedKeys = findSharedKeys();
                keyToFind.setSharedKeys(sharedKeys);
                assert_precondition(sharedKeys || gDisableNecessarySharedKeysCheck);
            }
            if (_usuallyTrue(sharedKeys != nullptr)) {
                // Look for a numeric key first:
                if (_usuallyTrue(keyToFind._hasNumericKey))
                    return get(keyToFind._numericKey, returnUndefined);
                // Key was not registered last we checked; see if dict contains any new keys:
                if (_usuallyFalse(_count == 0))
                    return nullptr;
                if (lookupSharedKey(keyToFind._rawString, sharedKeys, keyToFind._numericKey)) {
                    keyToFind._hasNumericKey = true;
                    return get(keyToFind._numericKey, returnUndefined);
                }
            }

            // Look up by string:
            const Value *key = findKeyByHint(keyToFind);
            if (!key)
                key = findKeyBySearch(keyToFind);
            return finishGet(key, keyToFind, returnUndefined);
        }

        key_t encodeKey(slice keyString, SharedKeys *sharedKeys) const noexcept {
            int intKey;
            if (lookupSharedKey(keyString, sharedKeys, intKey) && search(intKey))
                return intKey;
            else
                return keyString;
        }

        bool hasParent() const {
            return _usuallyTrue(_count > 0) && _usuallyFalse(Dict::isMagicParentKey(_first));
        }

        const Dict* getParent() const {
            return hasParent() ? (const Dict*)deref(second()) : nullptr;
        }


        __hot
        static int compareKeys(slice keyToFind, const Value *key) {
            if (_usuallyTrue(key->isInteger()))
                return 1;
            else
                return keyToFind.compare(keyBytes(key));
        }

        __hot
        static int compareKeys(int keyToFind, const Value *key) {
            assert_precondition(key->tag() == kShortIntTag || key->tag() == kStringTag
                                                           || key->tag() >= kPointerTagFirst);
            // This is optimized using the knowledge that short ints have a tag of 0.
            uint8_t hiByte = key->_byte[0];
            if (_usuallyTrue(hiByte <= 0x07))
                return keyToFind - ((hiByte << 8) | key->_byte[1]);     // positive int key
            else if (_usuallyFalse(hiByte <= 0x0F))
                return keyToFind - (int16_t)(0xF0 | (hiByte << 8) | key->_byte[1]); // negative
            else
                return -1;                                              // string, or ptr to string
        }

        __hot
        static int compareKeys(const Value *keyToFind, const Value *key) {
            if (keyToFind->tag() == kStringTag)
                return compareKeys(keyBytes(keyToFind), key);
            else
                return compareKeys((int)keyToFind->asInt(), key);
        }


    private:

        // typical binary search function; returns pointer to the key it finds
        template <class T, class CMP>
        __hot
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

        __hot
        const Value* findKeyByHint(Dict::key &keyToFind) const {
            if (keyToFind._hint < _count) {
                const Value *key  = offsetby(_first, keyToFind._hint * 2 * kWidth);
                if (compareKeys(keyToFind._rawString, key) == 0)
                    return key;
            }
            return nullptr;
        }

        // Finds a key in a dictionary via binary search of the UTF-8 key strings.
        __hot
        const Value* findKeyBySearch(Dict::key &keyToFind) const {
            auto key = search(keyToFind._rawString, [](slice target, const Value *val) {
                return compareKeys(target, val);
            });
            if (!key)
                return nullptr;

            // Found it! Cache dict index as optimization for next time:
            keyToFind._hint = (uint32_t)indexOf(key) / 2;
            return key;
        }

        __hot
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

        __hot
        static inline slice keyBytes(const Value *key) {
            return deref(key)->getStringBytes();
        }

        __hot
        static inline const Value* next(const Value *v) {
            return v->next<WIDE>();
        }

        __hot
        static inline const Value* deref(const Value *v) {
            return v->deref<WIDE>();
        }

        static constexpr size_t kWidth = (WIDE ? 4 : 2);
        static constexpr uint32_t kPtrMask = (WIDE ? 0x80000000 : 0x8000);
    };


    __hot
    int compareKeys(const Value *keyToFind, const Value *key, bool wide) {
        if (wide)
            return dictImpl<true>::compareKeys(keyToFind, key);
        else
            return dictImpl<true>::compareKeys(keyToFind, key);
    }


    void Dict::key::setSharedKeys(SharedKeys *sk) {
        assert_precondition(!_sharedKeys);
        _sharedKeys = retain(sk);
    }

    Dict::key::~key() {
        release(_sharedKeys);
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
        if (_usuallyFalse(imp._count >= 1 && isMagicParentKey(imp._first))) {
            // Dict has a parent; this makes counting much more expensive!
            uint32_t c = 0;
            for (iterator i(this); i; ++i)
                ++c;
            return c;
        } else {
            return imp._count;
        }
    }

    bool Dict::empty() const noexcept {
        if (_usuallyFalse(isMutable()))
            return heapDict()->empty();
        return countIsZero();
    }

    __hot
    const Value* Dict::get(slice keyToFind, bool returnUndefined) const noexcept {
        if (_usuallyFalse(isMutable()))
            return heapDict()->get(keyToFind);
        if (isWideArray())
            return dictImpl<true>(this).get(keyToFind, nullptr, returnUndefined);
        else
            return dictImpl<false>(this).get(keyToFind, nullptr, returnUndefined);
    }

    __hot
    const Value* Dict::get(int keyToFind, bool returnUndefined) const noexcept {
        if (_usuallyFalse(isMutable()))
            return heapDict()->get(keyToFind);
        else if (isWideArray())
            return dictImpl<true>(this).get(keyToFind, returnUndefined);
        else
            return dictImpl<false>(this).get(keyToFind, returnUndefined);
    }

    const Value* Dict::get(key &keyToFind, bool returnUndefined) const noexcept {
        if (_usuallyFalse(isMutable()))
            return heapDict()->get(keyToFind);
        else if (isWideArray())
            return dictImpl<true>(this).get(keyToFind);
        else
            return dictImpl<false>(this).get(keyToFind);
    }

    __hot
    const Value* Dict::get(const key_t &keyToFind, bool returnUndefined) const noexcept {
        if (_usuallyFalse(isMutable()))
            return heapDict()->get(keyToFind);
        else if (keyToFind.shared())
            return get(keyToFind.asInt(), returnUndefined);
        else
            return get(keyToFind.asString(), returnUndefined);
    }

    key_t Dict::encodeKey(slice keyString, SharedKeys *sharedKeys) const noexcept {
        if (isWideArray())
            return dictImpl<true>(this).encodeKey(keyString, sharedKeys);
        else
            return dictImpl<false>(this).encodeKey(keyString, sharedKeys);
    }

    MutableDict* Dict::asMutable() const noexcept {
        return isMutable() ? (MutableDict*)this : nullptr;
    }

    HeapDict* Dict::heapDict() const noexcept {
        return (HeapDict*)internal::HeapCollection::asHeapValue(this);
    }

    const Dict* Dict::getParent() const noexcept {
        if (isMutable())
            return heapDict()->source();
        else if (isWideArray())
            return dictImpl<true>(this).getParent();
        else
            return dictImpl<false>(this).getParent();
    }

    bool Dict::isEqualToDict(const Dict* dv) const noexcept {
        Dict::iterator i(this);
        Dict::iterator j(dv);
        if (!this->getParent() && !dv->getParent() && i.count() != j.count())
            return false;
        if (sharedKeys() == dv->sharedKeys()) {
            // If both dicts use same sharedKeys, their keys must be in the same order.
            for (; i; ++i, ++j)
                if (i.keyString() != j.keyString() || !i.value()->isEqual(j.value()))
                    return false;
        } else {
            unsigned n = 0;
            for (; i; ++i, ++n) {
                auto dvalue = dv->get(i.keyString());
                if (!dvalue || !i.value()->isEqual(dvalue))
                    return false;
            }
            if (dv->count() != n)
                return false;
        }
        return true;
    }

    EVEN_ALIGNED static constexpr Dict kEmptyDictInstance;
    const Dict* const Dict::kEmpty = &kEmptyDictInstance;


#pragma mark - DICT::ITERATOR:


    DictIterator::DictIterator(const Dict* d) noexcept
    :DictIterator(d, nullptr)
    { }

    DictIterator::DictIterator(const Dict* d, const SharedKeys *sk) noexcept
    :_a(d), _sharedKeys(sk)
    {
        readKV();
        if (_usuallyFalse(_key && Dict::isMagicParentKey(_key))) {
            _parent.reset( new DictIterator(_value->asDict()) );
            ++(*this);
        }
    }

    DictIterator::DictIterator(const Dict* d, bool) noexcept
    :_a(d)
    {
        readKV();
        // skips the parent check, so it will iterate the raw contents
    }

    SharedKeys* DictIterator::findSharedKeys() const {
        auto sk = Doc::sharedKeys(_a._first);
        _sharedKeys = sk;
        assert_precondition(sk || gDisableNecessarySharedKeysCheck);
        return sk;
    }

    slice DictIterator::keyString() const noexcept {
        slice keyStr = _key->asString();
        if (!keyStr && _key->isInteger()) {
            auto sk = _sharedKeys ? _sharedKeys : findSharedKeys();
            if (!sk)
                return nullslice;
            keyStr = sk->decode((int)_key->asInt());
        }
        return keyStr;
    }

    key_t DictIterator::keyt() const noexcept {
        if (_key->isInteger())
            return (int)_key->asInt();
        else
            return _key->asString();
    }

    DictIterator& DictIterator::operator++() {
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

    DictIterator& DictIterator::operator += (uint32_t n) {
        throwIf(n > _a._count, OutOfRange, "iterating past end of dict");
        _a._count -= n;
        _a._first = offsetby(_a._first, 2*_a._width*n);
        readKV();
        return *this;
    }

    void DictIterator::readKV() noexcept {
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

} }
