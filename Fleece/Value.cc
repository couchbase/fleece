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

    // Maps from typeCode to valueType
    static uint8_t kValueTypes[] = {
        kNull,
        kBoolean, kBoolean,
        kNumber, kNumber, kNumber, kNumber, kNumber, kNumber, kNumber, kNumber, // int1-8
        kNumber,
        kNumber, kNumber,
        kNumber,
        kDate,
        kString, kString, kString, kString,
        kData,
        kArray,
        kDict
    };

    static bool isNumeric(slice s);
    static double readNumericString(slice str);

#pragma mark - VALUE:

    valueType value::type() const {
        return _typeCode < sizeof(kValueTypes) ? (valueType)kValueTypes[_typeCode] : kNull;
    }

    uint32_t value::getParam() const {
        uint32_t param;
        GetUVarInt32(slice(_paramStart, kMaxVarintLen32), &param);
        return param;
    }

    uint32_t value::getParam(const uint8_t* &after) const {
        uint32_t param;
        after = _paramStart + GetUVarInt32(slice(_paramStart, kMaxVarintLen32), &param);
        return param;
    }

    uint64_t value::getParam64(const uint8_t* &after) const {
        uint64_t param;
        after = _paramStart + GetUVarInt(slice(_paramStart, kMaxVarintLen64), &param);
        return param;
    }

    const value* value::next() const {
        const uint8_t* end = _paramStart;
        switch (_typeCode) {
            case kNullCode...kTrueCode:
                return (const value*)(end + 0);
            case kInt1Code...kInt8Code:
                return (const value*)(end + 1 + _typeCode - kInt1Code);
            case kUInt64Code:
            case kFloat64Code:
                return (const value*)(end + 8);
            case kFloat32Code:
                return (const value*)(end + 4);

            case kDateCode:
                (void)getParam64(end);
                break;

            case kRawNumberCode:
            case kStringCode:
            case kSharedStringCode:
            case kDataCode: {
                uint32_t dataLength = getParam(end);
                end += dataLength;
                break;
            }
            case kSharedStringRefCode:
            case kExternStringRefCode:
                (void)getParam(end);
                break;

            case kArrayCode: {
                // This is somewhat expensive: have to traverse all values in the array
                uint32_t n = getParam(end);
                const value* v = (const value*)end;
                for (; n > 0; --n)
                    v = v->next();
                return v;
            }
            case kDictCode: {
                // This is somewhat expensive: have to traverse all keys+values in the dict
                uint32_t count;
                const value* key = ((const dict*)this)->firstKey(count);
                for (; count > 0; --count)
                    key = key->next()->next();
                return key;
            }
            default:
                throw "bad typecode";
        }
        return (const value*)end;
    }

    bool value::asBool() const {
        switch (_typeCode) {
            case kNullCode:
            case kFalseCode:
                return false;
                break;
            case kInt1Code...kRawNumberCode:
                return asInt() != 0;
            default:
                return true;
        }
    }

    int64_t value::asInt() const {
        switch (_typeCode) {
            case kNullCode:
            case kFalseCode:
                return 0;
            case kTrueCode:
                return 1;
            case kInt1Code...kInt8Code:
                return GetIntOfLength(_paramStart, _typeCode - (kInt1Code-1));
            case kUInt64Code:
                return (int64_t) _decLittle64(*(int64_t*)_paramStart);
            case kFloat32Code:
                return (int64_t) *(littleEndianFloat*)_paramStart;
            case kFloat64Code:
                return (int64_t) *(littleEndianDouble*)_paramStart;
            case kRawNumberCode:
                return (int64_t) readNumericString(asString());
            case kDateCode:
                return asDate();
            default:
                throw "value is not a number";
        }
    }

    uint64_t value::asUnsigned() const {
        if (_typeCode == kUInt64Code)
            return (uint64_t) _decLittle64(*(int64_t*)_paramStart);
        else
            return (uint64_t) asInt();
    }

    double value::asDouble() const {
        switch (_typeCode) {
            case kFloat32Code:
                return *(littleEndianFloat*)_paramStart;
            case kFloat64Code:
                return *(littleEndianDouble*)_paramStart;
            case kRawNumberCode:
                return readNumericString(asString());
            default:
                return (double)asInt();
        }
    }

    std::time_t value::asDate() const {
        if (_typeCode != kDateCode)
            throw "value is not a date";
        uint64_t param;
        //FIX: Should read signed varint
        GetUVarInt(slice(_paramStart, kMaxVarintLen32), &param);
        return param;
        return (std::time_t)param;
    }

    std::string value::toString() const {
        char str[32];
        switch (_typeCode) {
            case kNullCode:
                return "null";
            case kFalseCode:
                return "false";
            case kTrueCode:
                return "true";
            case kInt1Code...kInt8Code:
                sprintf(str, "%lld", asInt());
                break;
            case kUInt64Code:
                sprintf(str, "%llu", asUnsigned());
                break;
            case kFloat32Code:
                sprintf(str, "%.6f", (float)*((littleEndianFloat*)_paramStart));
                break;
            case kFloat64Code:
                sprintf(str, "%.16lf", asDouble());
                break;
            case kDateCode: {
                std::time_t date = asDate();
                std::strftime(str, sizeof(str), "\"%Y-%m-%dT%H:%M:%SZ\"", std::gmtime(&date));
                break;
            }
            default:
                return (std::string)asString();
        }
        return std::string(str);
    }

    slice value::asString(const stringTable* externStrings) const {
        const uint8_t* payload;
        uint32_t param = getParam(payload);
        switch (_typeCode) {
            case kStringCode:
            case kSharedStringCode:
            case kDataCode:
            case kRawNumberCode:
                return slice(payload, (size_t)param);
            case kSharedStringRefCode: {
                const value* str = (const value*)offsetby(this, -(ptrdiff_t)param);
                if (str->_typeCode != kSharedStringCode)
                    throw "invalid shared-string";
                param = str->getParam(payload);
                return slice(payload, (size_t)param);
            }
            case kExternStringRefCode:
                if (!externStrings)
                    throw "can't dereference extern string without table";
                if (param < 1 || param > externStrings->size())
                    throw "invalid extern string index";
                return (*externStrings)[param-1];
            default:
                throw "value is not a string";
        }
    }

    uint64_t value::stringToken() const {
        switch (_typeCode) {
            case kSharedStringCode:
                return (uint64_t)this;              // Shared string: return pointer to this
            case kSharedStringRefCode:
                return (uint64_t)this - getParam(); // Shared ref: return pointer to original string
            case kExternStringRefCode:
                return getParam();                  // Extern string: return code
            default:
                return 0;
        }
    }

    const array* value::asArray() const {
        if (_typeCode != kArrayCode)
            throw "value is not array";
        return (const array*)this;
    }

    const dict* value::asDict() const {
        if (_typeCode != kDictCode)
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
        if (s.size < 1)
            return false;
        const void *valueStart = s.buf;
        typeCode type = *(const typeCode*)valueStart;
        s.moveStart(1);  // consume type-code

        switch (type) {
            case kNullCode...kTrueCode:
                return true;
            case kInt1Code...kInt8Code: {
                size_t size = type - (kInt1Code - 1);
                return s.checkedMoveStart(size);
            }
            case kFloat32Code:
                return s.checkedMoveStart(4);
            case kUInt64Code:
            case kFloat64Code:
                return s.checkedMoveStart(8);
            case kDateCode: {
                uint64_t date;
                return ReadUVarInt(&s, &date);
            }
            case kRawNumberCode: {
                uint32_t length;
                return ReadUVarInt32(&s, &length) && isNumeric(s.read(length));
            }
            case kStringCode:
            case kSharedStringCode:         //TODO: Check for valid UTF-8 data
            case kDataCode: {
                uint32_t length;
                return ReadUVarInt32(&s, &length) && s.checkedMoveStart(length);
            }
            case kExternStringRefCode: {
                uint32_t ref;
                return ReadUVarInt32(&s, &ref);
            }
            case kSharedStringRefCode: {
                uint32_t param;
                if (!ReadUVarInt32(&s, &param))
                    return false;
                // Get pointer to original string:
                slice origString;
                origString.buf = offsetby(valueStart, -(ptrdiff_t)param);
                if (origString.buf < start || origString.buf >= s.buf)
                    return false;
                // Check that it's marked as a shared string:
                if (*(const typeCode*)origString.buf != kSharedStringCode)
                    return false;
                // Validate it:
                origString.setEnd(s.end());
                return validate(start, origString);
            }
            case kArrayCode: {
                uint32_t count;
                if (!ReadUVarInt32(&s, &count))
                    return false;
                for (; count > 0; --count)
                    if (!validate(start, s))
                        return false;
                return true;
            }
            case kDictCode: {
                uint32_t count;
                if (!ReadUVarInt32(&s, &count))
                    return false;
                // Skip hash codes:
                if (!s.checkedMoveStart(count*sizeof(uint16_t)))
                    return false;
                for (; count > 0; --count) {
                    auto v = (const value*)s.buf;
                    if (!validate(start, s) || (v->type() != kString))
                        return false;
                    if (!validate(start, s))
                        return false;
                }
                return true;
            }
            default:
                return false;
        }
    }

#pragma mark - ARRAY:

    const value* array::first() const {
        const uint8_t* f;
        (void)getParam(f);
        return (const value*)f;
    }

    const value* array::first(uint32_t &count) const {
        const uint8_t* f;
        count = getParam(f);
        return count ? (const value*)f : NULL;
    }

    array::iterator& array::iterator::operator++() {
        if (_count == 0)
            throw "iterating past end of array";
        if (--_count > 0)
            _value = _value->next();
        else
            _value = NULL;
        return *this;
    }

#pragma mark - DICT:

    uint16_t dict::hashCode(slice s) {
        uint32_t result;
        ::MurmurHash3_x86_32(s.buf, (int)s.size, 0, &result);
        return (uint16_t)_encLittle16(result & 0xFFFF);
    }

    const value* dict::get(slice keyToFind,
                           uint16_t hashToFind,
                           const stringTable* externStrings) const
    {
        const uint8_t* after;
        uint32_t count = getParam(after);
        auto hashes = (const uint16_t*)after;

        uint32_t keyIndex = 0;
        const value* key = (const value*)&hashes[count];
        for (uint32_t i = 0; i < count; i++) {
            if (hashes[i] == hashToFind) {
                while (keyIndex < i) {
                    key = key->next()->next();
                    ++keyIndex;
                }
                if (keyToFind.compare(key->asString(externStrings)) == 0)
                    return key->next();
            }
        }
        return NULL;
    }

    const value* dict::firstKey(uint32_t &count) const {
        const uint8_t* after;
        count = getParam(after);
        return (value*) offsetby(after, count * sizeof(uint16_t));
    }

    dict::iterator::iterator(const dict* d) {
        _key = d->firstKey(_count);
        _value = _count > 0 ? _key->next() : NULL;
    }

    dict::iterator& dict::iterator::operator++() {
        if (_count == 0)
            throw "iterating past end of dict";
        if (--_count > 0) {
            _key = _value->next();
            _value = _key->next();
        }
        return *this;
    }

#pragma mark - UTILITIES:

    static bool isNumeric(slice s) {
        if (s.size < 1)
            return false;
        for (const char* c = (const char*)s.buf; s.size > 0; s.size--, c++) {
            if (!isdigit(*c) && *c != '.' && *c != '+' && *c != '-' && *c != 'e' &&  *c != 'E')
                return false;
        }
        return true;
    }

    static double readNumericString(slice str) {
        char* cstr = strndup((const char*)str.buf, str.size);
        if (!cstr)
            return 0.0;
        char* eof;
        double result = ::strtod(cstr, &eof);
        if (eof - cstr != str.size)
            result = NAN;
        ::free(cstr);
        return result;
    }
    


}
