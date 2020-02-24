//
// ValueSlot.cc
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

#include "ValueSlot.hh"
#include "HeapArray.hh"
#include "HeapDict.hh"
#include "Encoder.hh"
#include "varint.hh"
#include <algorithm>

namespace fleece { namespace impl {
    using namespace std;
    using namespace internal;


    ValueSlot::ValueSlot()
    :_slot(nullptr)
    { }


    ValueSlot::ValueSlot(Null)
    :_slot({(kSpecialTag << 4) | kSpecialValueNull})
    { }


    ValueSlot::ValueSlot(HeapCollection *md)
    :_slot( retain(md)->asValue() )
    { }


    ValueSlot::ValueSlot(const ValueSlot &other) noexcept {
        *this = other; // invoke operator=, below
    }


    ValueSlot& ValueSlot::operator= (const ValueSlot &other) noexcept {
        releaseValue();
        _slot = other._slot;
        if (_slot.isPointer())
            retain(_slot.pointerValue());
        return *this;
    }


    ValueSlot::ValueSlot(ValueSlot &&other) noexcept {
        _slot = other._slot;
        other._slot = nullptr;
    }

    ValueSlot& ValueSlot::operator= (ValueSlot &&other) noexcept {
        release(_slot.asPointer());
        _slot = other._slot;
        other._slot = nullptr;
        return *this;
    }



    ValueSlot::~ValueSlot() {
        if (_slot.isPointer())
            release(_slot.pointerValue());
    }


    Retained<HeapCollection> HeapCollection::mutableCopy(const Value *v, tags ifType) {
        if (!v || v->tag() != ifType)
            return nullptr;
        if (v->isMutable())
            return (HeapCollection*)asHeapValue(v);
        switch (ifType) {
            case kArrayTag: return new HeapArray((const Array*)v);
            case kDictTag:  return new HeapDict((const Dict*)v);
            default:        return nullptr;
        }
    }


    void ValueSlot::releaseValue() {
        if (_slot.isPointer()) {
            release(_slot.pointerValue());
            _slot = nullptr;
        }
    }


    const Value* ValueSlot::asValue() const {
        return _slot.isPointer() ? _slot.pointerValue() : _slot.inlinePointer();
    }

    const Value* ValueSlot::asValueOrUndefined() const {
        if (_slot.isInline())
            return (const Value*)_slot.inlineBytes().buf;
        else if (_slot.asPointer())
            return _slot.asPointer();
        else
            return Value::kUndefinedValue;
    }

    HeapCollection* ValueSlot::asMutableCollection() const {
        const Value *ptr = _slot.asPointer();
        if (ptr && ptr->isMutable())
            return (HeapCollection*)HeapValue::asHeapValue(ptr);
        return nullptr;
    }


    static inline FLPURE uint8_t tagByte(internal::tags t, int tiny) {
        return uint8_t((t << 4) | tiny);
    }


    void ValueSlot::setInline(internal::tags valueTag, int tiny) {
        releaseValue();
        _slot.setInline({tagByte(valueTag, tiny)});
    }

    void ValueSlot::set(Null) {
        setInline(kSpecialTag, kSpecialValueNull);
    }


    void ValueSlot::set(bool b) {
        setInline(kSpecialTag, b ? kSpecialValueTrue : kSpecialValueFalse);
    }


    void ValueSlot::set(int i)       {setInt(i, false);}
    void ValueSlot::set(unsigned i)  {setInt(i, true);}
    void ValueSlot::set(int64_t i)   {setInt(i, false);}
    void ValueSlot::set(uint64_t i)  {setInt(i, true);}

#ifdef _MSC_VER
#pragma warning(push)
// unary minus operator applied to unsigned type, result still unsigned
// short circuited by isUnsigned
#pragma warning(disable : 4146)
#endif

    template <class INT>
    void ValueSlot::setInt(INT i, bool isUnsigned) {
        if (i < 2048 && (isUnsigned || -i < 2048)) {
            _slot.setInline({tagByte(kShortIntTag, (i >> 8) & 0x0F),  (uint8_t)(i & 0xFF)});
        } else {
            uint8_t buf[8];
            auto size = PutIntOfLength(buf, i, isUnsigned);
            setValue(kIntTag,
                     (int)(size-1) | (isUnsigned ? 0x08 : 0),
                     {buf, size});
        }
    }

#ifdef _MSC_VER
#pragma warning(pop)
#endif


    void ValueSlot::set(float f) {
        if (Encoder::isIntRepresentable(f)) {
            set((int32_t)f);
        } else {
            struct {
                uint8_t filler = 0;
                endian::littleEndianFloat le;
            } data;
            data.le = f;
            setValue(kFloatTag, 0, {(char*)&data.le - 1, sizeof(data.le) + 1});
        }
        assert_postcondition(asValue()->asFloat() == f);
    }

    void ValueSlot::set(double d) {
        if (Encoder::isIntRepresentable(d)) {
            set((int64_t)d);
        } else {
            struct {
                uint8_t filler = 0;
                endian::littleEndianDouble le;
            } data;
            data.le = d;
            setValue(kFloatTag, 8, {(char*)&data.le - 1, sizeof(data.le) + 1});
        }
        assert_postcondition(asValue()->asDouble() == d);
    }


    void ValueSlot::setValue(const Value *v) {
        if (_slot.isPointer()) {
            if (v == _slot.pointerValue())
                return;
            release(_slot.pointerValue());
        }
        if (v && v->tag() < kArrayTag) {
            auto size = v->dataSize();
            if (size <= kInlineCapacity) {
                // Copy value inline if it's small enough
                _slot.setInline(slice(v, size));
                return;
            }
        }
        // else point to it
        _slot.setPointer(retain(v));
    }


    void ValueSlot::setValue(tags valueTag, int tiny, slice bytes) {
        if (1 + bytes.size <= kInlineCapacity) {
            setInline(valueTag, tiny);
            memcpy(offsetby(_slot.inlinePointer(), 1), bytes.buf, bytes.size);
        } else {
            releaseValue();
            _slot.setPointer( retain(HeapValue::create(valueTag, tiny, bytes)->asValue()) );
        }
    }


    void ValueSlot::_setStringOrData(tags valueTag, slice s) {
        if (s.size + 1 <= kInlineCapacity) {
            // Short strings can go inline:
            setInline(valueTag, (int)s.size);
            memcpy(offsetby(_slot.inlinePointer(), 1), s.buf, s.size);
        } else {
            releaseValue();
            _slot.setPointer( retain(HeapValue::createStr(valueTag, s)->asValue()) );
        }
    }


    HeapCollection* ValueSlot::makeMutable(tags ifType) {
        if (_slot.isInline())
            return nullptr;
        Retained<HeapCollection> mval = HeapCollection::mutableCopy(_slot.pointerValue(), ifType);
        if (mval)
            set(mval->asValue());
        return mval;
    }


    void ValueSlot::copyValue(CopyFlags flags) {
        const Value *value = _slot.asPointer();
        if (_slot.isPointer() && value && ((flags & kCopyImmutables) || value->isMutable())) {
            bool recurse = (flags & kDeepCopy);
            Retained<HeapCollection> copy;
            switch (value->tag()) {
                case kArrayTag:
                    copy = new HeapArray((Array*)value);
                    if (recurse)
                        ((HeapArray*)copy.get())->copyChildren(flags);
                    set(copy->asValue());
                    break;
                case kDictTag:
                    copy = new HeapDict((Dict*)value);
                    if (recurse)
                        ((HeapDict*)copy.get())->copyChildren(flags);
                    set(copy->asValue());
                    break;
                case kStringTag:
                    set(value->asString());
                    break;
                case kFloatTag:
                    set(value->asDouble());
                    break;
                default:
                    assert(false);
            }
        }
    }

} }
