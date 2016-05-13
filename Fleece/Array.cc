//
//  Array.cc
//  Fleece
//
//  Created by Jens Alfke on 5/12/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#include "Array.hh"
#include "Internal.hh"
#include "varint.hh"
#include <assert.h>


namespace fleece {
    using namespace internal;


    const value* value::deref(const value *v, bool wide) {
        while (v->isPointer()) {
            v = derefPointer(v, wide);
            wide = true;                        // subsequent pointers must be wide
        }
        return v;
    }

    template <bool WIDE>
    const value* value::deref(const value *v) {
        if (v->isPointer()) {
            v = derefPointer<WIDE>(v);
            while (v->isPointer())
                v = derefPointer<true>(v);      // subsequent pointers must be wide
        }
        return v;
    }


#pragma mark - ARRAY:


    array::impl::impl(const value* v) {
        first = (const value*)(&v->_byte[2]);
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

    bool array::impl::next() {
        if (count == 0)
            throw "iterating past end of array";
        if (--count == 0)
            return false;
        first = first->next(wide);
        return true;
    }

    const value* array::impl::operator[] (unsigned index) const {
        if (index >= count)
            return NULL;
        if (wide)
            return value::deref<true> (offsetby(first, kWide   * index));
        else
            return value::deref<false>(offsetby(first, kNarrow * index));
    }

    size_t array::impl::indexOf(const value *v) const {
        return ((size_t)v - (size_t)first) / width(wide);
    }



    uint32_t array::count() const {
        return array::impl(this).count;
    }

    const value* array::get(uint32_t index) const {
        return impl(this)[index];
    }



    array::iterator::iterator(const array *a)
    :_a(a),
     _value(_a.firstValue())
    { }

    array::iterator& array::iterator::operator++() {
        _a.next();
        _value = _a.firstValue();
        return *this;
    }

    array::iterator& array::iterator::operator += (uint32_t n) {
        if (n > _a.count)
            throw "iterating past end of array";
        _a.count -= n;
        _a.first = offsetby(_a.first, width(_a.wide)*n);
        _value = _a.firstValue();
        return *this;
    }


#pragma mark - DICT:


    template <bool WIDE>
    struct dictImpl : public array::impl {

        dictImpl(const dict *d)
        :impl(d)
        { }

        const value* get_unsorted(slice keyToFind) const {
            const value *key = first;
            for (uint32_t i = 0; i < count; i++) {
                const value *val = next(key);
                if (keyToFind.compare(deref(key)->asString()) == 0)
                    return deref(val);
                key = next(val);
            }
            return NULL;
        }

        inline const value* get(slice keyToFind) const {
            auto key = (const value*) ::bsearch(&keyToFind, first, count, 2*kWidth, &keyCmp);
            if (!key)
                return NULL;
            return deref(next(key));
        }

        const value* get(dict::key &keyToFind) const {
            const value *start = first;

            // Use the index hint to possibly find the key in one probe:
            if (keyToFind._hint < count) {
                const value *key  = offsetby(start, keyToFind._hint * 2 * kWidth);
                if ((keyToFind._keyValue && key->isPointer() && deref(key) == keyToFind._keyValue)
                        || (keyCmp(&keyToFind._rawString, key) == 0)) {
                    return deref(next(key));
                }
                // If the hint failed, look it up the slow way...
            }

            // Check whether there is a cached key and, if so, whether it would be used in this dict:
            if (keyToFind._keyValue && (keyToFind._rawString.size >= kWidth)) {
                const value *key = start;
                const value *end = offsetby(key, count*2*kWidth);
                size_t maxOffset = (WIDE ?0xFFFFFFFF : 0xFFFF);
                auto offset = (size_t)((uint8_t*)key - (uint8_t*)keyToFind._keyValue);
                auto offsetAtEnd = (size_t)((uint8_t*)end - kWidth - (uint8_t*)keyToFind._keyValue);
                if (offset <= maxOffset && offsetAtEnd <= maxOffset) {
                    // OK, key value is in range so we can use it here, for a linear scan.
                    // Raw integer key we're looking for (in native byte order):
                    auto rawKeyToFind = (uint32_t)((offset >> 1) | kPtrMask);
                    while (key < end) {
                        const value *val = next(key);
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
            auto key = (const value*) ::bsearch(&keyToFind._rawString, start, count,
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
        static inline const value* next(const value *v) {
            return v->next<WIDE>();
        }

        static inline const value* deref(const value *v) {
            return value::deref<WIDE>(v);
        }

        static int keyCmp(const void* keyToFindP, const void* keyP) {
            const value *key = deref((const value*)keyP);
            return ((slice*)keyToFindP)->compare(key->asString());
        }

        static const size_t kWidth = (WIDE ? 4 : 2);
        static const uint32_t kPtrMask = (WIDE ? 0x80000000 : 0x8000);
    };



    uint32_t dict::count() const {
        return array::impl(this).count;
    }
    
    const value* dict::get_unsorted(slice keyToFind) const {
        if (isWideArray())
            return dictImpl<true>(this).get_unsorted(keyToFind);
        else
            return dictImpl<false>(this).get_unsorted(keyToFind);
    }

    const value* dict::get(slice keyToFind) const {
        if (isWideArray())
            return dictImpl<true>(this).get(keyToFind);
        else
            return dictImpl<false>(this).get(keyToFind);
    }

    const value* dict::get(key &keyToFind) const {
        if (isWideArray())
            return dictImpl<true>(this).get(keyToFind);
        else
            return dictImpl<false>(this).get(keyToFind);
    }



    dict::iterator::iterator(const dict* d)
    :_a(d)
    {
        readKV();
    }

    dict::iterator& dict::iterator::operator++() {
        if (_a.count == 0)
            throw "iterating past end of dict";
        --_a.count;
        _a.first = offsetby(_a.first, 2*width(_a.wide));
        readKV();
        return *this;
    }

    dict::iterator& dict::iterator::operator += (uint32_t n) {
        if (n > _a.count)
            throw "iterating past end of dict";
        _a.count -= n;
        _a.first = offsetby(_a.first, 2*width(_a.wide)*n);
        readKV();
        return *this;
    }

    void dict::iterator::readKV() {
        if (_a.count) {
            _key   = deref(_a.first,                _a.wide);
            _value = deref(_a.first->next(_a.wide), _a.wide);
        } else {
            _key = _value = NULL;
        }
    }

}
