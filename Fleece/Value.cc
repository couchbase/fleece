//
//  Value.cc
//  Fleece
//
//  Created by Jens Alfke on 1/26/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#include "Value.hh"
#include "Internal.hh"
#include "Endian.h"
#include "varint.hh"
#include <math.h>
#include <assert.h>


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
        if (tag() == kSpecialTag) {
            switch (tinyValue()) {
                case kSpecialValueNull:
                    return kNull;
                case kSpecialValueFalse...kSpecialValueTrue:
                    return kBoolean;
                default:
                    throw "unrecognized special value";
            }
        } else {
            return kValueTypes[tag()];
        }
    }


    bool value::asBool() const {
        switch (tag()) {
            case kSpecialTag:
                return tinyValue() >= kSpecialValueTrue;
            case kShortIntTag...kFloatTag:
                return asInt() != 0;
            default:
                return true;
        }
    }

    int64_t value::asInt() const {
        switch (tag()) {
            case kSpecialTag:
                return tinyValue() >= kSpecialValueTrue;
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
                throw "value is not a number";
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
                    return *(const littleEndianDouble*)&_byte[2];
                else
                    return *(const littleEndianFloat*)&_byte[2];
            }
            default:
                if (isUnsigned())
                    return asUnsigned();
                else
                    return asInt();
        }
    }

    std::string value::toString() const {
        char str[32];
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
                        return "null";
                    case kSpecialValueFalse:
                        return "false";
                    case kSpecialValueTrue:
                        return "true";
                    default:
                        return "???";
                }
                break;
            }
            case kFloatTag: {
                double f = asDouble();
                if (tinyValue() <= 4)
                    sprintf(str, "%.6f", f);
                else
                    sprintf(str, "%.16f", f);
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
            throw "value is not array";
        return (const array*)this;
    }

    const dict* value::asDict() const {
        if (tag() != kDictTag)
            throw "value is not dict";
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

#pragma mark - ARRAY:

    const value* value::arrayFirstAndCount(uint32_t *pCount) const {
        const uint8_t *first = &_byte[2];
        uint32_t count = shortValue() & 0x07FF;
        if (count == 0x07FF) {
            uint32_t realCount;
            size_t countSize = GetUVarInt32(slice(first, 10), &realCount);
            first += countSize + (countSize & 1);
            count = realCount;
        }
        *pCount = count;
        return count ? (const value*)first : NULL;
    }

    uint32_t value::arrayCount() const {
        uint32_t count;
        arrayFirstAndCount(&count);
        return count;
    }

    const value* value::deref(bool wide) const {
        const value *v = this;
        while (v->_byte[0] >= (kPointerTagFirst << 4)) {
            int32_t offset;
            if (wide) {
                offset = (int32_t)_dec32(*(uint32_t*)_byte) << 1;
            } else {
                int16_t off16 = _dec16(*(uint16_t*)_byte) << 1;
                offset = off16;
            }
            v = (const value*)offsetby(v, offset);
        }
        return v;
    }

    const value* array::get(uint32_t index) const {
        uint32_t count;
        const value* v = arrayFirstAndCount(&count);
        if (index >= count)
            throw "array index out of range";
        v += index;
        bool wide = isWideArray();
        if (wide)
            v += index;
        return v->deref(wide);
    }


    array::iterator::iterator(const array *a) {
        _p = a->arrayFirstAndCount(&_count);
        _wide = a->isWideArray();
        _value = _p ? _p->deref(_wide) : NULL;
    }

    array::iterator& array::iterator::operator++() {
        if (_count == 0)
            throw "iterating past end of array";
        if (--_count > 0) {
            if (_wide)
                ++_p;
            _value = (++_p)->deref(_wide);
        } else
            _p = _value = NULL;
        return *this;
    }

#pragma mark - DICT:

    const value* dict::get(slice keyToFind) const {
        bool wide = isWideArray();
        unsigned scale = wide ? 2 : 1;
        uint32_t count;
        const value* key = arrayFirstAndCount(&count);
        for (uint32_t i = 0; i < count; i++) {
            if (keyToFind.compare(key->deref(wide)->asString()) == 0) {
                return key + scale*count;  // i.e. value at index i
            }
            key += scale;
        }
        return NULL;
    }

    dict::iterator::iterator(const dict* d) {
        _pKey = d->arrayFirstAndCount(&_count);
        _wide = d->isWideArray();
        if (_pKey) {
            _pValue = _pKey + _count;
            _key = _pKey->deref(_wide);
            _value = _pValue->deref(_wide);
        } else {
            _key = _value = NULL;
        }
    }

    dict::iterator& dict::iterator::operator++() {
        if (_count == 0)
            throw "iterating past end of dict";
        if (--_count > 0) {
            if (_wide) {
                ++_pKey;
                ++_pValue;
            }
            _key = (++_pKey)->deref(_wide);
            _value = (++_pValue)->deref(_wide);
        } else {
            _key = _value = NULL;
        }
        return *this;
    }

}
