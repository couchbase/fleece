//
//  Value.cc
//  Fleece
//
//  Created by Jens Alfke on 1/26/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "Value.hh"
#include "Internal.hh"
#include "Endian.hh"
#include "varint.hh"
#include <assert.h>
#include <math.h>


namespace fleece {

    using namespace internal;

    // Maps from tag to valueType
    static const valueType kValueTypes[] = {
        kNumber, // small int
        kNumber, // int
        kNumber, // float
        kNull,   // special -- may also be kBoolean
        kString,
        kData,
        kArray,
        kDict,
        kNull   // pointer; should never be seen
    };


#pragma mark - VALUE:

    valueType value::type() const {
        auto t = tag();
        if (t == kSpecialTag) {
            switch (tinyValue()) {
                case kSpecialValueFalse...kSpecialValueTrue:
                    return kBoolean;
                case kSpecialValueNull:
                default:
                    return kNull;
            }
        } else {
            return kValueTypes[t];
        }
    }


    bool value::asBool() const {
        switch (tag()) {
            case kSpecialTag:
                return tinyValue() == kSpecialValueTrue;
            case kShortIntTag...kFloatTag:
                return asInt() != 0;
            default:
                return true;
        }
    }

    int64_t value::asInt() const {
        switch (tag()) {
            case kSpecialTag:
                return tinyValue() == kSpecialValueTrue;
            case kShortIntTag: {
                uint16_t i = shortValue();
                if (i & 0x0800)
                    return (int16_t)(i | 0xF000);   // sign-extend negative number
                else
                    return i;
            }
            case kIntTag: {
                int64_t n = 0;
                unsigned byteCount = tinyValue();
                if ((byteCount & 0x8) == 0) {       // signed integer
                    if (_byte[1+byteCount] & 0x80)  // ...and sign bit is set
                        n = -1;
                } else {
                    byteCount &= 0x7;
                }
                memcpy(&n, &_byte[1], ++byteCount);
                return _decLittle64(n);
            }
            case kFloatTag:
                return (int64_t)round(asDouble());
            default:
                return 0;
        }
    }

    // Explicitly instantiate both needed versions:
    template float value::asFloatOfType<float>() const;
    template double value::asFloatOfType<double>() const;

    template<typename T>
    T value::asFloatOfType() const {
        switch (tag()) {
            case kFloatTag: {
                if (_byte[0] & 0x8)
                    return (T)*(const littleEndianDouble*)&_byte[2];
                else
                    return (T)*(const littleEndianFloat*)&_byte[2];
            }
            default:
                if (isUnsigned())
                    return asUnsigned();
                else
                    return asInt();
        }
    }

    std::string value::toString() const {
        char buf[32], *str = buf;
        switch (tag()) {
            case kShortIntTag:
            case kIntTag: {
                int64_t i = asInt();
                if (isUnsigned())
                    sprintf(str, "%llu", (uint64_t)i);
                else
                    sprintf(str, "%lld", i);
                break;
            }
            case kSpecialTag: {
                switch (tinyValue()) {
                    case kSpecialValueNull:
                        str = (char*)"null";
                        break;
                    case kSpecialValueFalse:
                        str = (char*)"false";
                        break;
                    case kSpecialValueTrue:
                        str = (char*)"true";
                        break;
                    default:
                        str = (char*)"???";
                        break;
                }
                break;
            }
            case kFloatTag: {
                if (_byte[0] & 0x8)
                    sprintf(str, "%.16g", asDouble());
                else
                    sprintf(str, "%.6g", asFloat());
                break;
            }
            default:
                return (std::string)asString();
        }
        return std::string(str);
    }

    slice value::asString() const {
        switch (tag()) {
            case kStringTag:
            case kBinaryTag: {
                slice s(&_byte[1], tinyValue());
                if (s.size == 0x0F) {
                    // This means the actual length follows as a varint:
                    uint64_t realLength;
                    ReadUVarInt(&s, &realLength);
                    s.size = realLength;
                }
                return s;
            }
            default:
                return slice::null;
        }
    }

    const array* value::asArray() const {
        if (tag() != kArrayTag)
            return NULL;
        return (const array*)this;
    }

    const dict* value::asDict() const {
        if (tag() != kDictTag)
            return NULL;
        return (const dict*)this;
    }

#pragma mark - VALIDATION:

    const value* value::fromTrustedData(slice s) {
        // Root value is at the end of the data and is two bytes wide:
        assert(validate(s)); // validate anyway, in debug builds; abort if invalid
        return deref<false>(rootPointer(s));
    }

    const value* value::fromData(slice s) {
        return validate(s) ? fromTrustedData(s) : NULL;
    }

    bool value::validate(slice s) {
        if (s.size < 2 || (s.size % 2))
            return false;
        return rootPointer(s)->validate(s.buf, s.end(), false);
    }

    bool value::validate(const void *dataStart, const void *dataEnd, bool wide) const {
        // TODO: watch out for integer overflow/wraparound in 32-bit.
        // First dereference a pointer:
        if (isPointer()) {
            auto derefed = derefPointer(this, wide);
            return derefed >= dataStart && derefed->validate(dataStart, this, true);
        }
        auto t = tag();
        size_t size = dataSize();
        if (t == kArrayTag || t == kDictTag) {
            wide = isWideArray();
            size_t itemCount = arrayCount();
            if (t == kDictTag)
                itemCount *= 2;
            // Check that size fits:
            size += itemCount * (wide ? kWide : kNarrow);
            if (offsetby(this, size) > dataEnd)
                return false;

            // Check each array/dict element:
            if (itemCount > 0) {
                auto item = getArrayInfo().first;
                while (itemCount-- > 0) {
                    auto second = item->next(wide);
                    if (!item->validate(dataStart, second, wide))
                        return false;
                    item = second;
                }
            }
            return true;
        } else {
            // Non-collection; just check that size fits:
            return offsetby(this, size) <= dataEnd;
        }
    }


#pragma mark - POINTERS:

    const value* value::deref(const value *v, bool wide) {
        while (v->isPointer()) {
            v = derefPointer(v, wide);
            wide = true;
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


    value::arrayInfo value::getArrayInfo() const {
        const uint8_t *first = &_byte[2];
        uint32_t count = shortValue() & 0x07FF;
        if (count == 0x07FF) {
            size_t countSize = GetUVarInt32(slice(first, 10), &count);
            first += countSize + (countSize & 1);
        }
        return arrayInfo{(const value*)first, count, isWideArray()};
    }

    const value* array::get(uint32_t index) const {
        return getArrayInfo()[index];
    }


    array::iterator::iterator(const array *a) {
        _a = a->getArrayInfo();
        _value = _a.firstValue();
    }

    array::iterator& array::iterator::operator++() {
        _a.next();
        _value = _a.firstValue();
        return *this;
    }


#pragma mark - DICT:

    const value* dict::get_unsorted(slice keyToFind) const {
        auto info = getArrayInfo();
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
        auto info = getArrayInfo();
        auto key = (const value*) ::bsearch(&keyToFind, info.first, info.count,
                                            (WIDE ?8 :4), &keyCmp<WIDE>);
        if (!key)
            return NULL;
        return deref<WIDE>(key->next<WIDE>());
    }

    const value* dict::get(slice keyToFind) const {
        return isWideArray() ? get<true>(keyToFind) : get<false>(keyToFind);
    }

    dict::iterator::iterator(const dict* d) {
        _a = d->getArrayInfo();
        readKV();
    }

    dict::iterator& dict::iterator::operator++() {
        if (_a.count == 0)
            throw "iterating past end of dict";
        --_a.count;
        _a.first = offsetby(_a.first, _a.wide ? 2*kWide : 2*kNarrow);
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
