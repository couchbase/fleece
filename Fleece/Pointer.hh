//
// Pointer.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "Value.hh"

namespace fleece { namespace internal {

    class Pointer : public Value {
    public:
        static constexpr size_t kMaxNarrowOffset = 0x7FFE;

        Pointer(size_t offset, int width, bool external =false)
        :Value(kPointerTagFirst, 0)
        {
            assert((offset & 1) == 0);
            offset >>= 1;
            if (width < internal::kWide) {
                throwIf(offset >= 0x4000, InternalError, "offset too large");
                if (external)
                    offset |= 0x4000;
                setNarrowBytes((uint16_t)_enc16(offset | 0x8000)); // big-endian, high bit set
            } else {
                if (offset >= 0x40000000)
                    FleeceException::_throw(OutOfRange, "data too large");
                if (external)
                    offset |= 0x40000000;
                setWideBytes((uint32_t)_enc32(offset | 0x80000000));
            }
        }

        bool isExternal() const                 {return (_byte[0] & 0x4000) != 0;}


        // Returns the byte offset
        template <bool WIDE>
        uint32_t offset() const noexcept {
            if (WIDE)
                return (_dec32(wideBytes()) & ~0xC0000000) << 1;
            else
                return (_dec16(narrowBytes()) & ~0xC000) << 1;
        }


        template <bool WIDE>
        const Value* deref() const {
            assert(offset<WIDE>() > 0);
            assert(!isExternal());
            return offsetby(this, -(ptrdiff_t)offset<WIDE>());
        }

        const Value* derefWide() const  {return deref<true>();}       // just a compiler workaround


        const Value* deref(bool wide) const {
            return wide ? deref<true>() : deref<false>();
        }


        // assumes data is untrusted, and double-checks offsets for validity.
        const Value* carefulDeref(bool wide,
                                  const void *dataStart, const void *dataEnd) const noexcept
        {
            auto target = deref(wide);
            if (_usuallyFalse(target < dataStart) || _usuallyFalse(target >= dataEnd))
                return nullptr;
            while (_usuallyFalse(target->isPointer())) {
                auto target2 = target->_asPointer()->deref<true>();
                if (_usuallyFalse(target2 < dataStart) || _usuallyFalse(target2 >= target))
                    return nullptr;
                target = target2;
            }
            return target;
        }

    private:
        void setNarrowBytes(uint16_t b)             {*(uint16_t*)_byte = b;}
        void setWideBytes(uint32_t b)               {*(uint32_t*)_byte = b;}
        uint16_t narrowBytes() const                {return *(uint16_t*)_byte;}
        uint32_t wideBytes() const                  {return *(uint32_t*)_byte;}
    };


} }
