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

    
    value::arrayInfo::arrayInfo(const value* v) {
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

    bool value::arrayInfo::next() {
        if (count == 0)
            throw "iterating past end of array";
        if (--count == 0)
            return false;
        first = first->next(wide);
        return true;
    }

    const value* value::arrayInfo::operator[] (unsigned index) const {
        if (index >= count)
            return NULL;
        if (wide)
            return deref<true> (offsetby(first, kWide   * index));
        else
            return deref<false>(offsetby(first, kNarrow * index));
    }

    size_t value::arrayInfo::indexOf(const value *v) const {
        return ((size_t)v - (size_t)first) / width(wide);
    }


    const value* array::get(uint32_t index) const {
        return arrayInfo(this)[index];
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

    const value* dict::get_unsorted(slice keyToFind) const {
        arrayInfo info(this);
        const value *key = info.first;
        for (uint32_t i = 0; i < info.count; i++) {
            auto val = key->next(info.wide);
            if (keyToFind.compare(deref(key, info.wide)->asString()) == 0)
                return deref(val, info.wide);
            key = val->next(info.wide);
        }
        return NULL;
    }

    template <bool WIDE>
    int dict::keyCmp(const void* keyToFindP, const void* keyP) {
        const value *key = value::deref<WIDE>((const value*)keyP);
        return ((slice*)keyToFindP)->compare(key->asString());
    }

    template <bool WIDE>
    inline const value* dict::get(slice keyToFind) const {
        arrayInfo info(this);
        auto key = (const value*) ::bsearch(&keyToFind, info.first, info.count,
                                            2*width(WIDE), &keyCmp<WIDE>);
        if (!key)
            return NULL;
        return deref<WIDE>(key->next<WIDE>());
    }

    const value* dict::get(slice keyToFind) const {
        return isWideArray() ? get<true>(keyToFind) : get<false>(keyToFind);
    }

    template <bool WIDE>
    const value* dict::get(const arrayInfo &info, dict::key &keyToFind) const {
        const value *start = info.first;

        // Use the index hint to possibly find the key in one probe:
        if (keyToFind._hint < info.count) {
            const value *key  = offsetby(start, keyToFind._hint * 2 * width(WIDE));
            if ((keyToFind._keyValue && key->isPointer() && deref<WIDE>(key) == keyToFind._keyValue)
                    || (dict::keyCmp<WIDE>(&keyToFind._rawString, key) == 0)) {
                return deref<WIDE>(key->next<WIDE>());
            }
            // If the hint failed, look it up the slow way...
        }

        // Check whether there is a cached key and, if so, whether it would be used in this dict:
        if (keyToFind._keyValue && (keyToFind._rawString.size > (WIDE ? 3 : 1))) {
            const value *key = start;
            const value *end = offsetby(key, info.count*2*width(WIDE));
            size_t maxOffset = (WIDE ?0xFFFFFFFF : 0xFFFF);
            auto offset = (size_t)((uint8_t*)key - (uint8_t*)keyToFind._keyValue);
            auto offsetAtEnd = (size_t)((uint8_t*)end - width(WIDE) - (uint8_t*)keyToFind._keyValue);
            if (offset <= maxOffset && offsetAtEnd <= maxOffset) {
                // OK, key value is in range so we can use it here, for a linear scan.
                // Raw integer key we're looking for (in native byte order):
                auto rawKeyToFind16 = (uint16_t)((offset >> 1) | 0x8000);
                auto rawKeyToFind32 = (uint32_t)((offset >> 1) | 0x80000000);

                while (key < end) {
                    const value *val = key->next<WIDE>();
                    if (WIDE ? (_dec32(*(uint32_t*)key) == rawKeyToFind32)
                             : (_dec16(*(uint16_t*)key) == rawKeyToFind16)) {
                        // Found it! Cache the dict index as a hint for next time:
                        keyToFind._hint = (uint32_t)info.indexOf(key) / 2;
                        return deref<WIDE>(val);
                    }
                    rawKeyToFind16 += kNarrow;      // offset to string increases as key advances
                    rawKeyToFind32 += kWide;
                    key = val->next<WIDE>();
                }
                return NULL;
            }
        }

        // Can't use the encoded key, so fall back to binary search by string bytes:
        auto key = (const value*) ::bsearch(&keyToFind._rawString, start, info.count,
                                            2*width(WIDE), &keyCmp<WIDE>);
        if (!key)
            return NULL;

        // Found it! Cache dict index and encoded key as optimizations for next time:
        if (key->isPointer())
            keyToFind._keyValue = deref<WIDE>(key);
        keyToFind._hint = (uint32_t)info.indexOf(key) / 2;
        return deref<WIDE>(key->next<WIDE>());
    }

    const value* dict::get(key &k) const {
        arrayInfo info(this);
        return info.wide ? get<true>(info, k) : get<false>(info, k);
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
