//
// Value.cc
//
// Copyright (c) 2015 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "Value.hh"
#include "Pointer.hh"
#include "Array.hh"
#include "Dict.hh"
#include "Internal.hh"
#include "Doc.hh"
#include "HeapValue.hh"
#include "Endian.hh"
#include "FleeceException.hh"
#include "varint.hh"
#include "PlatformCompat.hh"
#include "JSONEncoder.hh"
#include "ParseDate.hh"
#include <math.h>
#include "betterassert.hh"


namespace fleece { namespace impl {

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


    class ConstValue : public Value {
    public:
        constexpr ConstValue(internal::tags tag, int tiny, int byte1 = 0)
        :Value(tag, tiny, byte1) { }
    };

    EVEN_ALIGNED static constexpr const ConstValue
        kNullInstance           {kSpecialTag, kSpecialValueNull},
        kUndefinedInstance      {kSpecialTag, kSpecialValueUndefined};

    const Value* const Value::kNullValue      = &kNullInstance;
    const Value* const Value::kUndefinedValue = &kUndefinedInstance;


#pragma mark - TYPE CHECK / CONVERSION:

    valueType Value::type() const noexcept {
        auto t = tag();
        if (_usuallyFalse(t == kSpecialTag)) {
            switch (tinyValue()) {
                case kSpecialValueFalse:
                case kSpecialValueTrue:
                    return kBoolean;
                case kSpecialValueNull:
                case kSpecialValueUndefined:
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
            uint32_t length;
            size_t lengthBytes = GetUVarInt32(s, &length);
            return slice(&s[lengthBytes], length);
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
                switch (tinyValue()) {
                    case kSpecialValueNull:
                        str = (char*)"null";
                        break;
                    case kSpecialValueUndefined:
                        str = (char*)"undefined";
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
                    WriteDouble(asDouble(), str, 32);
                else
                    WriteFloat(asFloat(), str, 32);
                break;
            }
            default:
                return alloc_slice(asString());
        }
        return alloc_slice(str);
    }

    slice Value::asString() const noexcept {
        return _usuallyTrue(tag() == kStringTag) ? getStringBytes() : slice();
    }

    slice Value::asData() const noexcept {
        return _usuallyTrue(tag() == kBinaryTag) ? getStringBytes() : slice();
    }

    int64_t Value::asTimestamp() const noexcept {
        switch (tag()) {
            case kStringTag:
                return ParseISO8601Date(asString());
            case kShortIntTag:
            case kIntTag:
            case kFloatTag:
                return asInt();
            default:
                return kInvalidDate;
        }
    }

    const Array* Value::asArray() const noexcept {
        if (_usuallyFalse(tag() != kArrayTag))
            return nullptr;
        return (const Array*)this;
    }

    const Dict* Value::asDict() const noexcept {
        if (_usuallyFalse(tag() != kDictTag))
            return nullptr;
        return (const Dict*)this;
    }


    SharedKeys* Value::sharedKeys() const noexcept {
        return Doc::sharedKeys(this);
    }


    template <int VER>
    alloc_slice Value::toJSON(bool canonical) const {
        JSONEncoder encoder;
        if (VER >= 5)
            encoder.setJSON5(true);
        encoder.setCanonical(canonical);
        encoder.writeValue(this);
        return encoder.finish();
    }


    // Explicitly instantiate both needed versions of the templates:
    template alloc_slice Value::toJSON<1>(bool canonical) const;
    template alloc_slice Value::toJSON<5>(bool canonical) const;


    std::string Value::toJSONString() const {
        return toJSON().asString();
    }


    bool Value::isEqual(const Value *v) const {
        if (!v || _byte[0] != v->_byte[0])
            return false;
        if (_usuallyFalse(this == v))
            return true;
        switch (tag()) {
            case kShortIntTag:
            case kIntTag:
                return asInt() == v->asInt();
            case kFloatTag:
                if (isDouble())
                    return asDouble() == v->asDouble();
                else
                    return asFloat() == v->asFloat();
            case kSpecialTag:
                return _byte[1] == v->_byte[1];
            case kStringTag:
            case kBinaryTag:
                return getStringBytes() == v->getStringBytes();
            case kArrayTag: {
                Array::iterator i((const Array*)this);
                Array::iterator j((const Array*)v);
                if (i.count() != j.count())
                    return false;
                for (; i; ++i, ++j)
                    if (!i.value()->isEqual(j.value()))
                        return false;
                return true;
            }
            case kDictTag:
                return ((const Dict*)this)->isEqualToDict((const Dict*)v);
            default:
                return false;
        }
    }


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
        assert(((size_t)s.buf & 1) == 0);  // Values must be 2-byte aligned

        // Reject obviously invalid data (odd address, too short, or odd length)
        if (_usuallyFalse((size_t)s.buf & 1) || _usuallyFalse(s.size < kNarrow)
                                             || _usuallyFalse(s.size % kNarrow))
            return nullptr;
        // Root value is at the end of the data and is two bytes wide:
        auto root = (const Value*)offsetby(s.buf, s.size - internal::kNarrow);
        if (_usuallyTrue(root->isPointer())) {
            // If the root is a pointer, sanity-check the destination, then deref:
            const void *dataStart = s.buf, *dataEnd = root;
            return root->_asPointer()->carefulDeref(false, dataStart, dataEnd);
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
                auto itemsSize = itemCount * array._width;
                if (_usuallyFalse(offsetby(array._first, itemsSize) > dataEnd))
                    return false;

                // Check each Array/Dict element:
                auto item = array._first;
                while (itemCount-- > 0) {
                    auto nextItem = offsetby(item, array._width);
                    if (item->isPointer()) {
                        if (_usuallyFalse(!item->_asPointer()->validate(array._width == kWide, dataStart)))
                            return false;
                    } else {
                        if (_usuallyFalse(!item->validate(dataStart, nextItem)))
                            return false;
                    }
                    item = nextItem;
                }
                return true;
            }
        }
        // Default: just check that size fits:
        return offsetby(this, dataSize()) <= dataEnd;
    }

    // This does not include the inline items in arrays/dicts
    size_t Value::dataSize() const noexcept {
        switch(tag()) {
            case kShortIntTag:
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

#pragma mark - POINTERS:


    const Value* Value::deref(bool wide) const {
        if (!isPointer())
            return this;
        auto v = _asPointer()->deref(wide);
        while (_usuallyFalse(v->isPointer()))
            v = v->_asPointer()->derefWide();      // subsequent pointers must be wide
        return v;
    }

    template <bool WIDE>
    const Value* Value::deref() const {
        if (!isPointer())
            return this;
        auto v = _asPointer()->deref<WIDE>();
        while (!WIDE && _usuallyFalse(v->isPointer()))
            v = v->_asPointer()->derefWide();      // subsequent pointers must be wide
        return v;
    }

    // Explicitly instantiate both needed versions:
    template const Value* Value::deref<false>() const;
    template const Value* Value::deref<true>() const;


    void Value::_retain() const     {HeapValue::retain(this);}
    void Value::_release() const    {HeapValue::release(this);}

} }
