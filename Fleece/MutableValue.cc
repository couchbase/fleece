//
// MutableValue.cc
//
// Copyright © 2018 Couchbase. All rights reserved.
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

#include "MutableValue.hh"
#include "MutableArray.hh"
#include "MutableDict.hh"
#include "varint.hh"
#include <algorithm>

namespace fleece { namespace internal {
    using namespace std;
    using namespace internal;


    MutableValue::MutableValue(Null)
    :_inline(true)
    ,_asInline{(kSpecialTag << 4) | kSpecialValueNull}
    { }

    MutableValue::~MutableValue() {
        if (_usuallyFalse(_malloced))
            free(const_cast<Value*>(_asValue));
    }


    MutableCollection* MutableCollection::mutableCopy(const Value *v, tags ifType) {
        if (!v || v->tag() != ifType)
            return nullptr;
        if (v->isMutable())
            return asMutable(v);
        switch (ifType) {
            case kArrayTag: return new MutableArray((const Array*)v);
            case kDictTag:  return new MutableDict((const Dict*)v);
            default:        return nullptr;
        }
    }


    void MutableValue::reset() {
        if (_usuallyFalse(_malloced)) {
            free(const_cast<Value*>(_asValue));
            _malloced = false;
        }
    }


    const Value* MutableValue::asValue() const {
        return _inline ? (const Value*)&_asInline : _asValue;
    }


    void MutableValue::setInline(internal::tags valueTag, int tiny) {
        reset();
        _inline = true;
        _asInline[0] = uint8_t((valueTag << 4) | tiny);
    }

    void MutableValue::set(Null) {
        setInline(kSpecialTag, kSpecialValueNull);
    }


    void MutableValue::set(bool b) {
        setInline(kSpecialTag, b ? kSpecialValueTrue : kSpecialValueFalse);
    }


    void MutableValue::set(int i)       {setInt(i, false);}
    void MutableValue::set(unsigned i)  {setInt(i, true);}
    void MutableValue::set(int64_t i)   {setInt(i, false);}
    void MutableValue::set(uint64_t i)  {setInt(i, true);}

    template <class INT>
    void MutableValue::setInt(INT i, bool isUnsigned) {
        if (i < 2048 && (isUnsigned || -i < 2048)) {
            setInline(kShortIntTag, (i >> 8) & 0x0F);
            _asInline[1] = (uint8_t)(i & 0xFF);
        } else {
            uint8_t buf[8];
            auto size = PutIntOfLength(buf, i, isUnsigned);
            setValue(kIntTag,
                     (int)(size-1) | (isUnsigned ? 0x08 : 0),
                     {buf, size});
        }
    }


    void MutableValue::set(float f) {
        littleEndianFloat lf(f);
        setValue(kFloatTag, 0, {&lf, sizeof(lf)});
    }

    void MutableValue::set(double d) {
        littleEndianDouble ld(d);
        setValue(kFloatTag, 0, {&ld, sizeof(ld)});
    }


    void MutableValue::set(const Value *v) {
        reset();
        if (v && v->tag() < kArrayTag) {
            auto size = v->dataSize();
            if (size <= sizeof(_asInline)) {
                _inline = true;
                memcpy(&_asInline, v, size);
                return;
            }
        }
        _inline = false;
        _asValue = v;
    }


    uint8_t* MutableValue::allocateValue(size_t size) {
        reset();
        _asValue = (const Value*)slice::newBytes(size);
        _inline = false;
        _malloced = true;
        return (uint8_t*)_asValue;
    }


    void MutableValue::setValue(tags valueTag, int tiny, slice bytes) {
        uint8_t* dst;
        if (1 + bytes.size <= sizeof(_asInline)) {
            reset();
            dst = &_asInline[0];
            _inline = true;
        } else {
            dst = allocateValue(1 + bytes.size);
        }
        dst[0] = uint8_t((valueTag << 4) | tiny);
        memcpy(&dst[1], bytes.buf, bytes.size);
    }


    void MutableValue::_setStringOrData(tags valueTag, slice s) {
        if (s.size <= sizeof(_asInline) - 1) {
            // Short strings can go inline:
            setInline(valueTag, (int)s.size);
            memcpy(&_asInline[1], s.buf, s.size);
        } else {
            // Allocate a string Value on the heap. (Adapted from Encoder::writeData)
            auto buf = allocateValue(2 + kMaxVarintLen32 + s.size);
            (void) new (buf) Value (kStringTag, (uint8_t)min(s.size, (size_t)0xF));
            size_t sizeByteCount = 1;
            if (s.size >= 0x0F)
                sizeByteCount += PutUVarInt(&buf[1], s.size);
            memcpy(&buf[sizeByteCount], s.buf, s.size);
        }
    }


    MutableCollection* MutableValue::makeMutable(tags ifType) {
        if (_inline)
            return nullptr;
        auto mval = MutableCollection::mutableCopy(_asValue, ifType);
        if (mval)
            set(mval);
        return mval;
    }

} }
