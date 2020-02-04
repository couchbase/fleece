//
// Pointer.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "Value.hh"
#include "betterassert.hh"

namespace fleece { namespace impl { namespace internal {
    using namespace fleece::impl;


    class Pointer : public Value {
    public:
        static constexpr size_t kMaxNarrowOffset = 0x7FFE;

        Pointer(size_t offset, int width, bool external =false);

        bool isExternal() const PURE                 {return (_byte[0] & 0x40) != 0;}

        // Returns the byte offset
        template <bool WIDE>
        PURE uint32_t offset() const noexcept {
            if (WIDE)
                return (_dec32(wideBytes()) & ~0xC0000000) << 1;
            else
                return (_dec16(narrowBytes()) & ~0xC000) << 1;
        }

        template <bool WIDE>
        inline const Value* deref() const noexcept {
            auto off = offset<WIDE>();
            assert(off > 0);
            const Value *dst = offsetby(this, -(ptrdiff_t)off);
            if (_usuallyFalse(isExternal()))
                dst = derefExtern(WIDE, dst);
            return dst;
        }

        const Value* derefWide() const noexcept           {return deref<true>();}       // just a workaround

        const Value* deref(bool wide) const noexcept  {
            return wide ? deref<true>() : deref<false>();
        }

        // assumes data is untrusted, and double-checks offsets for validity.
        const Value* carefulDeref(bool wide,
                                  const void* &dataStart,
                                  const void* &dataEnd) const noexcept;


        bool validate(bool wide, const void *dataStart) const noexcept PURE;

    private:
        // Byte offset as interpreted prior to the 'extern' flag
        template <bool WIDE>
        PURE uint32_t legacyOffset() const noexcept {
            if (WIDE)
                return (_dec32(wideBytes()) & ~0x80000000) << 1;
            else
                return (_dec16(narrowBytes()) & ~0x8000) << 1;
        }

        const Value* derefExtern(bool wide, const Value *dst) const noexcept;

        void setNarrowBytes(uint16_t b)             {*(uint16_t*)_byte = b;}
        void setWideBytes(uint32_t b)               {*(uint32_t*)_byte = b;}
        uint16_t narrowBytes() const PURE                {return *(uint16_t*)_byte;}
        uint32_t wideBytes() const PURE                  {return *(uint32_t*)_byte;}
    };


} } }
