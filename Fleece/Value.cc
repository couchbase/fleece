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
                if (_byte[0] & 0x8) {
                    littleEndianDouble d;
                    memcpy(&d, &_byte[2], sizeof(d));
                    return (T)d;
                } else {
                    littleEndianFloat f;
                    memcpy(&f, &_byte[2], sizeof(f));
                    return (T)f;
                }
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
                        throw "illegal special typecode";
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
                    uint32_t realLength;
                    ReadUVarInt32(&s, &realLength);
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
        assert(fromData(s) != NULL); // validate anyway, in debug builds; abort if invalid
        auto root = fastValidate(s);
        return root ? deref<true>(root) : NULL;
    }

    const value* value::fromData(slice s) {
        auto root = fastValidate(s);
        if (root && !root->validate(s.buf, s.end(), true))
            root = NULL;
        return root;
    }

    const value* value::fastValidate(slice s) {
        if (s.size < kNarrow || (s.size % kNarrow))
            return NULL;
        auto root = (const value*)offsetby(s.buf, s.size - internal::kNarrow);
        if (root->isPointer()) {
            // If the root is a pointer, sanity-check the destination:
            auto derefed = derefPointer<false>(root);
            if (derefed >= root || derefed < s.buf)
                return NULL;
            root = derefed;
            // The root itself might point to a wide pointer, if the actual value is too far away:
            if (root->isPointer()) {
                derefed = derefPointer<true>(root);
                if (derefed >= root || derefed < s.buf)
                    return NULL;
                root = derefed;
            }
        } else {
            // If the root is a direct value there better not be any data before it:
            if (s.size != kNarrow)
                return NULL;
        };
        return root;
    }

    bool value::validate(const void *dataStart, const void *dataEnd, bool wide) const {
        // First dereference a pointer:
        if (isPointer()) {
            auto derefed = derefPointer(this, wide);
            return derefed >= dataStart
                && derefed < this  // could fail if ptr wraps around past 0
                && derefed->validate(dataStart, this, true);
        }
        auto t = tag();
        size_t size = dataSize();
        if (t == kArrayTag || t == kDictTag) {
            wide = isWideArray();
            size_t itemCount = arrayCount();
            if (t == kDictTag)
                itemCount *= 2;
            // Check that size fits:
            size += itemCount * width(wide);
            if (offsetby(this, size) > dataEnd)
                return false;

            // Check each array/dict element:
            if (itemCount > 0) {
                auto item = arrayInfo(this).first;
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

    // This does not include the inline items in arrays/dicts
    size_t value::dataSize() const {
        switch(tag()) {
            case kShortIntTag:
            case kSpecialTag:   return 2;
            case kFloatTag:     return isDouble() ? 10 : 6;
            case kIntTag:       return 2 + (tinyValue() & 0x07);
            case kStringTag:
            case kBinaryTag:    return (uint8_t*)asString().end() - (uint8_t*)this;
            case kArrayTag:
            case kDictTag:      return (uint8_t*)arrayInfo(this).first - (uint8_t*)this;
            case kPointerTagFirst:
            default:            return 2;   // size might actually be 4; depends on context
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

}
