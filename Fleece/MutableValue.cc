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


    MutableValue::~MutableValue() {
        if (_usuallyFalse(_malloced))
            free(const_cast<Value*>(_asValue));
    }


    void MutableValue::reset() {
        if (_usuallyFalse(_malloced)) {
            free(const_cast<Value*>(_asValue));
            _malloced = false;
        }
    }


    const Value* MutableValue::asValuePointer() const {
        switch (_which) {
            case IsInline:                  return (const Value*)&_asInline;
            case IsValuePointer:            return _asValue;
            case IsMutableCollection:       return _asCollection->asValuePointer();
        }
    }


    MutableValue& MutableValue::operator= (const Value *v) {
        reset();
        if (!v || MutableCollection::isMutableCollection(v)) {
            _which = IsMutableCollection;
            _asCollection = (MutableCollection*)v;
        } else {
            auto size = v->dataSize();
            if (size <= sizeof(_asInline)) {
                _which = IsInline;
                memcpy(&_asInline, v, size);
            } else {
                _which = IsValuePointer;
                _asValue = v;
            }
        }
        return *this;
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
            setInline(kShortIntTag, (i >> 8) & 0x0F, i & 0xFF);
        } else {
            uint8_t buf[8];
            auto size = PutIntOfLength(buf, i, isUnsigned) - 1;
            setValue(kIntTag,
                     (int)size | (isUnsigned ? 0x08 : 0),
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
        if (MutableCollection::isMutableCollection(v)) {
            _which = IsMutableCollection;
            _asCollection = MutableCollection::asMutableCollection(v);
        } else {
            _which = IsValuePointer;
            _asValue = v;
        }
    }


    uint8_t* MutableValue::allocateValue(size_t size) {
        reset();
        _asValue = (const Value*)slice::newBytes(size);
        _which = IsValuePointer;
        _malloced = true;
        return (uint8_t*)_asValue;
    }


    void MutableValue::setValue(tags valueTag, int tiny, slice bytes) {
        uint8_t* dst;
        if (1 + bytes.size <= sizeof(_asInline)) {
            reset();
            dst = &_asInline[0];
            _which = IsInline;
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


    const Value* MutableValue::makeMutable(tags ifType) {
        switch (_which) {
            case IsInline:
                return nullptr;
            case IsValuePointer: {
                if (!_asValue || _asValue->tag() != ifType)
                    return nullptr;
                MutableCollection *newMutableResult;
                if (ifType == kArrayTag)
                    newMutableResult = new MutableArray((const Array*)_asValue);
                else
                    newMutableResult = new MutableDict((const Dict*)_asValue);
                set(newMutableResult);
                return newMutableResult->asValuePointer();
            }
            case IsMutableCollection: {
                if (!_asCollection || _asCollection->tag() != ifType)
                    return nullptr;
                return _asCollection->asValuePointer();
            }
        }
    }

} }
