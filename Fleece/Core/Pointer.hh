//
// Pointer.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "Value.hh"
#include "ExternResolver.hh"

namespace fleece { namespace internal {

    class Pointer : public Value {
    public:
        static constexpr size_t kMaxNarrowOffset = 0x7FFE;

        Pointer(size_t offset, int width, bool external =false);

        bool isExternal() const                 {return (_byte[0] & 0x40) != 0;}


        // Returns the byte offset
        template <bool WIDE>
        uint32_t offset() const noexcept {
            if (WIDE)
                return (_dec32(wideBytes()) & ~0xC0000000) << 1;
            else
                return (_dec16(narrowBytes()) & ~0xC000) << 1;
        }

        template <bool WIDE>
        const Value* deref() const              {return _deref(offset<WIDE>());}
        const Value* derefWide() const          {return deref<true>();}       // just a workaround


        const Value* deref(bool wide) const {
            return wide ? deref<true>() : deref<false>();
        }


        // assumes data is untrusted, and double-checks offsets for validity.
        const Value* carefulDeref(bool wide,
                                  const void* &dataStart,
                                  const void* &dataEnd) const noexcept;


        bool validate(bool wide, const void *dataStart) const noexcept;

    private:
        const Value* _deref(uint32_t offset) const;

        void setNarrowBytes(uint16_t b)             {*(uint16_t*)_byte = b;}
        void setWideBytes(uint32_t b)               {*(uint32_t*)_byte = b;}
        uint16_t narrowBytes() const                {return *(uint16_t*)_byte;}
        uint32_t wideBytes() const                  {return *(uint32_t*)_byte;}
    };


} }
