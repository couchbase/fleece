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
#include "PlatformCompat.hh"
#include "JSONEncoder.hh"
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
        if (_usuallyFalse(t == kSpecialTag)) {
            switch (_byte[0]) {
                case kSpecialValueFalse:
                case kSpecialValueTrue:
                    return kBoolean;
                case kSpecialValueMutableArray:
                    return kArray;
                case kSpecialValueMutableDict:
                    return kDict;
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
                return _byte[0] != kSpecialValueFalse && _byte[0] != kSpecialValueNull;
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
                return _byte[0] == kSpecialValueTrue;
            case kShortIntTag: {
                uint16_t i = shortValue();
                if (_usuallyFalse(i & 0x0800))
                    return (int16_t)(i | 0xF000);   // sign-extend negative number
                else
                    return i;
            }
            case kIntTag: {
                int64_t n = 0;
                unsigned byteCount = tinyValue();
                if ((byteCount & 0x8) == 0) {       // signed integer
                    if (((uint8_t*)&_byte)[1+byteCount] & 0x80)  // ...and sign bit is set
                        n = -1;
                } else {
                    byteCount &= 0x7;
                }
                memcpy(&n, &_byte[1], ++byteCount);
                return _decLittle64(n);
            }
            case kFloatTag:
                return (int64_t)asDouble();
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
            s.setSize(realLength);
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
                    sprintf(str, "%llu", (unsigned long long)i);
                else
                    sprintf(str, "%lld", (long long)i);
                break;
            }
            case kSpecialTag: {
                switch (_byte[0]) {
                    case kSpecialValueNull:
                        str = (char*)"null";
                        break;
                    case kSpecialValueFalse:
                        str = (char*)"false";
                        break;
                    case kSpecialValueTrue:
                        str = (char*)"true";
                        break;
                    case kSpecialValueMutableArray:
                    case kSpecialValueMutableDict:
                        return alloc_slice();
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
                return alloc_slice();
        }
        return alloc_slice(str);
    }

    slice Value::asString() const noexcept {
        return _usuallyTrue(tag() == kStringTag) ? getStringBytes() : nullslice;
    }

    slice Value::asData() const noexcept {
        return _usuallyTrue(tag() == kBinaryTag) ? getStringBytes() : nullslice;
    }

    const Array* Value::asArray() const noexcept {
        if (_usuallyFalse(tag() != kArrayTag && !isMutableArray()))
            return nullptr;
        return (const Array*)this;
    }

    MutableArray* Value::asMutableArray() const noexcept {
        if (_usuallyFalse(!isMutableArray()))
            return nullptr;
        return (MutableArray*)this;
    }

    const Dict* Value::asDict() const noexcept {
        if (_usuallyFalse(tag() != kDictTag && !isMutableDict()))
            return nullptr;
        return (const Dict*)this;
    }

    MutableDict* Value::asMutableDict() const noexcept {
        if (_usuallyFalse(!isMutableDict()))
            return nullptr;
        return (MutableDict*)this;
    }


    template <int VER>
    alloc_slice Value::toJSON(const SharedKeys *sk, bool canonical) const {
        JSONEncoder encoder;
        encoder.setSharedKeys(sk);
        if (VER >= 5)
            encoder.setJSON5(true);
        encoder.setCanonical(canonical);
        encoder.writeValue(this);
        return encoder.extractOutput();
    }


    // Explicitly instantiate both needed versions of the templates:
    template alloc_slice Value::toJSON<1>(const SharedKeys *sk, bool canonical) const;
    template alloc_slice Value::toJSON<5>(const SharedKeys *sk, bool canonical) const;


#pragma mark - VALIDATION:

    
    const Value* Value::fromTrustedData(slice s) noexcept {
        assert(fromData(s) != nullptr); // validate anyway, in debug builds; abort if invalid
        return findRoot(s);
    }

    const Value* Value::fromData(slice s) noexcept {
        auto root = findRoot(s);
        if (root && _usuallyFalse(!root->validate(s.buf, s.end())))
            root = nullptr;
        return root;
    }

    const Value* Value::findRoot(slice s) noexcept {
        // Root value is at the end of the data and is two bytes wide:
        if (_usuallyFalse(s.size < kNarrow) || _usuallyFalse(s.size % kNarrow))
            return nullptr;
        auto root = (const Value*)offsetby(s.buf, s.size - internal::kNarrow);
        if (_usuallyTrue(root->isPointer())) {
            // If the root is a pointer, sanity-check the destination, then deref:
            return root->carefulDeref(false, s.buf, root);
        } else {
            // If the root is a direct value there better not be any data before it:
            if (_usuallyFalse(s.size != kNarrow))
                return nullptr;
        };
        return root;
    }

    bool Value::validate(const void *dataStart, const void *dataEnd) const noexcept {
        auto t = tag();
        if (t == kArrayTag || t == kDictTag) {
            Array::impl array(this);
            if (_usuallyTrue(array._count > 0)) {
                // For validation purposes a Dict is just an array with twice as many items:
                size_t itemCount = array._count;
                if (_usuallyTrue(t == kDictTag))
                    itemCount *= 2;
                // Check that size fits:
                auto itemsSize = itemCount * width(array._wide);
                if (_usuallyFalse(offsetby(array._first, itemsSize) > dataEnd))
                    return false;

                // Check each Array/Dict element:
                auto item = array._first;
                while (itemCount-- > 0) {
                    auto nextItem = item->next(array._wide);
                    if (item->isPointer()) {
                        item = item->carefulDeref(array._wide, dataStart, this);
                        if (_usuallyFalse(item == nullptr))
                            return false;
                        if (_usuallyFalse(!item->validate(dataStart, this)))
                            return false;
                    } else {
                        if (_usuallyFalse(!item->validate(dataStart, nextItem)))
                            return false;
                    }
                    item = nextItem;
                }
                return true;
            }
        } else if (_usuallyFalse(t == kSpecialTag)) {
            if (_usuallyFalse(_byte[0] & 0x3))
                return false;       // Ephemeral value (mutable): illegal in stored data
        }
        // Default: just check that size fits:
        return offsetby(this, dataSize()) <= dataEnd;
    }

    // This does not include the inline items in arrays/dicts
    size_t Value::dataSize() const noexcept {
        switch(tag()) {
            case kShortIntTag:  return 2;
            case kSpecialTag:   return 2;
            case kFloatTag:     return isDouble() ? 10 : 6;
            case kIntTag:       return 2 + (tinyValue() & 0x07);
            case kStringTag:
            case kBinaryTag:    return (uint8_t*)getStringBytes().end() - (uint8_t*)this;
            case kArrayTag:
            case kDictTag:      return (uint8_t*)Array::impl(this)._first - (uint8_t*)this;
            case kPointerTagFirst:
            default:            return 2;   // size might actually be 4; depends on context
        }
    }

    const Value* Value::carefulDeref(bool wide,
                                     const void *dataStart, const void *dataEnd) const noexcept
    {
        auto target = derefPointer(this, wide);
        if (_usuallyFalse(target < dataStart) || _usuallyFalse(target >= dataEnd))
            return nullptr;
        while (_usuallyFalse(target->isPointer())) {
            auto target2 = derefPointer<true>(target);
            if (_usuallyFalse(target2 < dataStart) || _usuallyFalse(target2 >= target))
                return nullptr;
            target = target2;
        }
        return target;
    }


#pragma mark - POINTERS:


    const Value* Value::deref(const Value *v, bool wide) {
        if (v->isPointer()) {
            v = derefPointer(v, wide);
            while (_usuallyFalse(v->isPointer()))
                v = derefPointer<true>(v);      // subsequent pointers must be wide
        }
        return v;
    }

    template <bool WIDE>
    const Value* Value::deref(const Value *v) {
        if (v->isPointer()) {
            v = derefPointer<WIDE>(v);
            while (!WIDE && _usuallyFalse(v->isPointer()))
                v = derefPointer<true>(v);      // subsequent pointers must be wide
        }
        return v;
    }

    // Explicitly instantiate both needed versions:
    template const Value* Value::deref<false>(const Value *v);
    template const Value* Value::deref<true>(const Value *v);


}
