//
// Pointer.hh
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
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

        bool isExternal() const FLPURE                 {return (_byte[0] & 0x40) != 0;}

        // Returns the byte offset
        template <bool WIDE>
        FLPURE uint32_t offset() const noexcept {
            if (WIDE)
                return (endian::dec32(wideBytes()) & ~0xC0000000) << 1;
            else
                return (endian::dec16(narrowBytes()) & ~0xC000) << 1;
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


        bool validate(bool wide, const void *dataStart) const noexcept FLPURE;

    private:
        // Byte offset as interpreted prior to the 'extern' flag
        template <bool WIDE>
        FLPURE uint32_t legacyOffset() const noexcept {
            if (WIDE)
                return (endian::dec32(wideBytes()) & ~0x80000000) << 1;
            else
                return (endian::dec16(narrowBytes()) & ~0x8000) << 1;
        }

        const Value* derefExtern(bool wide, const Value *dst) const noexcept;

        void setNarrowBytes(uint16_t b)             {*(uint16_t*)_byte = b;}
        void setWideBytes(uint32_t b)               {*(uint32_t*)_byte = b;}
        uint16_t narrowBytes() const FLPURE                {return *(uint16_t*)_byte;}
        uint32_t wideBytes() const FLPURE                  {return *(uint32_t*)_byte;}
    };


} } }
