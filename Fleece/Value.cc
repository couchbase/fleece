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

    const value* value::fromData(slice s) {
        slice s2 = s;
        if (validate(s.buf, s2) && s2.size == 0)
            return (const value*)s.buf;
        else
            return NULL;
    }

    bool value::validate(const void *start, slice& s) {
        if (s.size < 2)
            return false;
        s.moveStart(s.size);
        return true;    //TODO
    }

#pragma mark - POINTERS:

    template <bool WIDE>
    const value* value::derefPointer(const value *v) {
        if (WIDE)
            return offsetby(v, _dec32(*(uint32_t*)v->_byte) << 1);
        else
            return offsetby(v, (int16_t)(_dec16(*(uint16_t*)v->_byte) << 1));
    }

    const value* value::deref(const value *v, bool wide) {
        while (v->isPointer())
            v = wide ? derefPointer<true>(v) : derefPointer<false>(v);
        return v;
    }

    template <bool WIDE>
    const value* value::deref(const value *v) {
        while (v->isPointer())
            v = derefPointer<WIDE>(v);
        return v;
    }

#pragma mark - ARRAY:

    value::arrayInfo value::getArrayInfo() const {
        const uint8_t *first = &_byte[2];
        uint32_t count = shortValue() & 0x07FF;
        if (count == 0x07FF) {
            size_t countSize = GetUVarInt32(slice(first, 10), &count);
            first += countSize + (countSize & 1);
        }
        return arrayInfo{(const value*)first, count};
    }

    const value* array::get(uint32_t index) const {
        auto info = getArrayInfo();
        if (index >= info.count)
            throw "array index out of range";
        auto v = info.first + index;
        bool wide = isWideArray();
        if (wide)
            v += index;
        return deref(v, wide);
    }


    array::iterator::iterator(const array *a) {
        _a = a->getArrayInfo();
        _wide = a->isWideArray();
        _value = _a.count ? deref(_a.first, _wide) : NULL;
    }

    array::iterator& array::iterator::operator++() {
        if (_a.count == 0)
            throw "iterating past end of array";
        if (--_a.count > 0) {
            if (_wide)
                ++_a.first;
            _value = deref(++_a.first, _wide);
        } else
            _a.first = _value = NULL;
        return *this;
    }

    const value* array::iterator::operator[] (unsigned index) {
        if (index >= _a.count)
            throw "array index out of range";
        if (_wide)
            index <<= 1;
        return deref(_a.first + index, _wide);
    }


#pragma mark - DICT:

    const value* dict::get(slice keyToFind) const {
        bool wide = isWideArray();
        unsigned scale = wide ? 2 : 1;
        uint32_t count;
        auto info = getArrayInfo();
        const value *key = info.first;
        for (uint32_t i = 0; i < info.count; i++) {
            if (keyToFind.compare(deref(key, wide)->asString()) == 0) {
                return key + scale*count;  // i.e. value at index i
            }
            key += scale;
        }
        return NULL;
    }

    template <bool WIDE>
    int dict::keyCmp(const void* keyToFindP, const void* keyP) {
        const value *key = value::deref<WIDE>((const value*)keyP);
        return ((slice*)keyToFindP)->compare(key->asString());
    }

    template <bool WIDE>
    inline const value* dict::get_sorted(slice keyToFind) const {
        uint32_t count;
        auto info = getArrayInfo();
        auto key = (const value*) ::bsearch(&keyToFind, info.first, info.count,
                                            (WIDE ?4 :2), &keyCmp<WIDE>);
        if (key)
            key += count * (WIDE ?2 :1);
        return key;
    }

    const value* dict::get_sorted(slice keyToFind) const {
        return isWideArray() ? get_sorted<true>(keyToFind) : get_sorted<false>(keyToFind);
    }


    dict::iterator::iterator(const dict* d) {
        _a = d->getArrayInfo();
        _wide = d->isWideArray();
        if (_a.count) {
            _pValue = _a.first + _a.count;
            _key = deref(_a.first, _wide);
            _value = deref(_pValue, _wide);
        } else {
            _key = _value = NULL;
        }
    }

    dict::iterator& dict::iterator::operator++() {
        if (_a.count == 0)
            throw "iterating past end of dict";
        if (--_a.count > 0) {
            if (_wide) {
                ++_a.first;
                ++_pValue;
            }
            _key = deref(++_a.first, _wide);
            _value = deref(++_pValue, _wide);
        } else {
            _key = _value = NULL;
        }
        return *this;
    }

}
