//
//  Value.cc
//  Fleece
//
//  Created by Jens Alfke on 1/26/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#include "Value.hh"
#include "Endian.h"
#include "varint.hh"
extern "C" {
#include "murmurhash3_x86_32.h"
}
#include <math.h>
#include <assert.h>


namespace fleece {

    // Maps from tag to valueType
    static valueType kValueTypes[] = {
        kInteger,
        kInteger,
        kFloat,
        kNull,
        kString,
        kData,
        kArray,
        kDict,
        kNull
    };


#pragma mark - VALUE:

    valueType value::type() const {
        if (tag() == kSpecialTag) {
            switch (shortValue()) {
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
                return shortValue() < kSpecialValueTrue;
            case kShortIntTag...kFloatTag:
                return asInt() != 0;
            default:
                return true;
        }
    }

    int64_t value::asInt() const {
        switch (tag()) {
            case kShortIntTag: {
                uint16_t i = shortValue();
                if (i & 0x0800)
                    return (int16_t)(i | 0xF000);   // sign-extend negative number
                else
                    return i;
            }
            case kIntTag: {
                int64_t n = 0;
                unsigned byteCount = tinyCount();
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

    double value::asDouble() const {
        switch (tag()) {
            case kFloatTag: {
                if (tinyCount() <= 4)
                    return *(const littleEndianFloat*)&_byte[2];
                else
                    return *(const littleEndianDouble*)&_byte[2];
            }
            default:
                if (isUnsigned())
                    return (double)asUnsigned();
                else
                    return (double)asInt();
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
                switch (shortValue()) {
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
                if (tinyCount() <= 4)
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
                slice s(&_byte[1], tinyCount());
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
        uint32_t count = shortValue();
        const uint8_t *first = &_byte[2];
        if (count == 0x0FFF) {
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

    const value* value::deref() const {
        const value *v = this;
        while (v->_byte[0] >= (kPointerTagFirst << 4)) {
            int16_t offset = (int16_t)_dec16(*(uint16_t*)_byte) << 1;
            v = (const value*)offsetby(v, offset);
        }
        return v;
    }

    const value* array::get(uint32_t index) const {
        const value* v = (value*)&_byte[2+2*index];
        return v->deref();
    }


    array::iterator::iterator(const array *a) {
        _p = a->arrayFirstAndCount(&_count);
        _value = _p ? _p->deref() : NULL;
    }

    array::iterator& array::iterator::operator++() {
        if (_count == 0)
            throw "iterating past end of array";
        if (--_count > 0)
            _value = (++_p)->deref();
        else
            _p = _value = NULL;
        return *this;
    }

#pragma mark - DICT:

    uint16_t dict::hashCode(slice s) {
        uint32_t result;
        ::MurmurHash3_x86_32(s.buf, (int)s.size, 0, &result);
        return (uint16_t)_encLittle16(result & 0xFFFF);
    }

    const value* dict::get(slice keyToFind) const {
        uint32_t count;
        const value* key = arrayFirstAndCount(&count);
        for (uint32_t i = 0; i < count; i++) {
            if (keyToFind.compare(key->deref()->asString()) == 0) {
                return key + count;  // i.e. value at index count
            }
            key++;
        }
        return NULL;
    }

    dict::iterator::iterator(const dict* d) {
        _pKey = d->arrayFirstAndCount(&_count);
        if (_pKey) {
            _pValue = _pKey + _count;
            _key = _pKey->deref();
            _value = _pValue->deref();
        } else {
            _key = _value = NULL;
        }
    }

    dict::iterator& dict::iterator::operator++() {
        if (_count == 0)
            throw "iterating past end of dict";
        if (--_count > 0) {
            _key = (++_pKey)->deref();
            _value = (++_pValue)->deref();
        } else {
            _key = _value = NULL;
        }
        return *this;
    }

}
