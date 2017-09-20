//
//  Dict.cc
//  Fleece
//
//  Created by Jens Alfke on 9/14/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "Dict.hh"
#include "DictImpl.hh"
#include "SharedKeys.hh"
#include "Internal.hh"
#include "PlatformCompat.hh"
#include <atomic>


namespace fleece {
    using namespace internal;

    
#pragma mark - DICT IMPLEMENTATION:


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


    static constexpr Dict kEmptyDictInstance;
    const Dict* const Dict::kEmpty = &kEmptyDictInstance;


#pragma mark - DICT::ITERATOR:


    Dict::iterator::iterator(const Dict* d) noexcept
    :_d(d)
    {
        readKV();
    }

    Dict::iterator::iterator(const Dict* d, const SharedKeys *sk) noexcept
    :_d(d), _sharedKeys(sk)
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
        throwIf(_d._count == 0, OutOfRange, "iterating past end of dict");
        --_d._count;
        _d._first = offsetby(_d._first, 2*width(_d._wide));
        readKV();
        return *this;
    }

    Dict::iterator& Dict::iterator::operator += (uint32_t n) {
        throwIf(n > _d._count, OutOfRange, "iterating past end of dict");
        _d._count -= n;
        _d._first = offsetby(_d._first, 2*width(_d._wide)*n);
        readKV();
        return *this;
    }

    void Dict::iterator::readKV() noexcept {
        if (_usuallyTrue(_d._count)) {
            _key   = deref(_d._first,                _d._wide);
            _value = deref(_d._first->next(_d._wide), _d._wide);
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


#ifndef NDEBUG
    namespace internal {
        std::atomic<unsigned> gTotalComparisons;
    }
#endif

}
