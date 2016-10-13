//
//  Value.cc
//  Fleece
//
//  Created by Jens Alfke on 1/26/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "Value.hh"
#include "Array.hh"
#include "Internal.hh"
#include "Endian.hh"
#include "FleeceException.hh"
#include "varint.hh"
#include "MSVC_Compat.hh"
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


#pragma mark - TYPE CHECK / CONVERSION:

    valueType Value::type() const noexcept {
        auto t = tag();
        if (t == kSpecialTag) {
            switch (tinyValue()) {
                case kSpecialValueFalse:
                case kSpecialValueTrue:
                    return kBoolean;
                case kSpecialValueNull:
                default:
                    return kNull;
            }
        } else {
            return kValueTypes[t];
        }
    }


    bool Value::asBool() const noexcept {
        switch (tag()) {
            case kSpecialTag:
                return tinyValue() == kSpecialValueTrue;
            case kShortIntTag:
            case kIntTag:
            case kFloatTag:
                return asInt() != 0;
            default:
                return true;
        }
    }

    int64_t Value::asInt() const noexcept {
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
    template float Value::asFloatOfType<float>() const noexcept;
    template double Value::asFloatOfType<double>() const noexcept;

    template<typename T>
    T Value::asFloatOfType() const noexcept {
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

    slice Value::getStringBytes() const noexcept {
        slice s(&_byte[1], tinyValue());
        if (_usuallyFalse(s.size == 0x0F)) {
            // This means the actual length follows as a varint:
            uint32_t realLength;
            ReadUVarInt32(&s, &realLength);
            s.size = realLength;
        }
        return s;
    }

    alloc_slice Value::toString() const {
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
                        str = (char*)"{?special?}";
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
                return alloc_slice(asString());
        }
        return alloc_slice(str);
    }

    slice Value::asString() const noexcept {
        switch (tag()) {
            case kStringTag:
            case kBinaryTag:
                return getStringBytes();
            default:
                return slice();
        }
    }

    const Array* Value::asArray() const noexcept {
        if (tag() != kArrayTag)
            return nullptr;
        return (const Array*)this;
    }

    const Dict* Value::asDict() const noexcept {
        if (tag() != kDictTag)
            return nullptr;
        return (const Dict*)this;
    }


#pragma mark - VALIDATION:

    
    const Value* Value::fromTrustedData(slice s) noexcept {
        // Root value is at the end of the data and is two bytes wide:
        assert(fromData(s) != nullptr); // validate anyway, in debug builds; abort if invalid
        auto root = fastValidate(s);
        return root ? deref<true>(root) : nullptr;
    }

    const Value* Value::fromData(slice s) noexcept {
        auto root = fastValidate(s);
        if (root && !root->validate(s.buf, s.end(), true))
            root = nullptr;
        return root;
    }

    const Value* Value::fastValidate(slice s) noexcept {
        if (s.size < kNarrow || (s.size % kNarrow))
            return nullptr;
        auto root = (const Value*)offsetby(s.buf, s.size - internal::kNarrow);
        if (root->isPointer()) {
            // If the root is a pointer, sanity-check the destination:
            auto derefed = derefPointer<false>(root);
            if (derefed >= root || derefed < s.buf)
                return nullptr;
            root = derefed;
            // The root itself might point to a wide pointer, if the actual value is too far away:
            if (root->isPointer()) {
                derefed = derefPointer<true>(root);
                if (derefed >= root || derefed < s.buf)
                    return nullptr;
                root = derefed;
            }
        } else {
            // If the root is a direct value there better not be any data before it:
            if (s.size != kNarrow)
                return nullptr;
        };
        return root;
    }

    bool Value::validate(const void *dataStart, const void *dataEnd, bool wide) const noexcept {
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
            size_t itemCount = ((const Array*)this)->count();
            if (t == kDictTag)
                itemCount *= 2;
            // Check that size fits:
            size += itemCount * width(wide);
            if (offsetby(this, size) > dataEnd)
                return false;

            // Check each Array/Dict element:
            if (itemCount > 0) {
                auto item = Array::impl(this)._first;
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
    size_t Value::dataSize() const noexcept {
        switch(tag()) {
            case kShortIntTag:
            case kSpecialTag:   return 2;
            case kFloatTag:     return isDouble() ? 10 : 6;
            case kIntTag:       return 2 + (tinyValue() & 0x07);
            case kStringTag:
            case kBinaryTag:    return (uint8_t*)asString().end() - (uint8_t*)this;
            case kArrayTag:
            case kDictTag:      return (uint8_t*)Array::impl(this)._first - (uint8_t*)this;
            case kPointerTagFirst:
            default:            return 2;   // size might actually be 4; depends on context
        }
    }

}
