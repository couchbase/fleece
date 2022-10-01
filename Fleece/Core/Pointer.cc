//
// Pointer.cc
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "Pointer.hh"
#include "Doc.hh"
#include <tuple>
#include <stdio.h>
#include "betterassert.hh"

using namespace std;

namespace fleece { namespace impl { namespace internal {

    Pointer::Pointer(size_t offset, int width, bool external)
    :Value(kPointerTagFirst, 0)
    {
        assert_precondition((offset & 1) == 0);
        offset >>= 1;
        if (width < internal::kWide) {
            throwIf(offset >= 0x4000, InternalError, "offset too large");
            if (_usuallyFalse(external))
                offset |= 0x4000;
            setNarrowBytes(endian::enc16(uint16_t(offset | 0x8000))); // big-endian, high bit set
        } else {
            if (_usuallyFalse(offset >= 0x40000000))
                FleeceException::_throw(OutOfRange, "data too large");
            if (_usuallyFalse(external))
                offset |= 0x40000000;
            setWideBytes(endian::enc32(uint32_t(offset | 0x80000000)));
        }
    }


    const Value* Pointer::derefExtern(bool wide, const Value *dst) const noexcept {
        // Resolve external pointer:
        dst = Doc::resolvePointerFrom(this, dst);
        if (_usuallyTrue(dst != nullptr))
            return dst;

        // Either invalid extern ref, or a legacy pointer without an 'extern' flag:
        if (!wide) {
            // Find the Scope I'm in and check if the legacy destination is within that scope too:
            dst = offsetby(this, -(ptrdiff_t)legacyOffset<false>());
            auto scope = Scope::containing(this);
            if (scope && scope->data().containsAddress(dst))
                return dst;
        }

        // Invalid extern:
        auto off = wide ? offset<true>() : offset<false>();
        fprintf(stderr, "FATAL: Fleece extern pointer at %p, offset -%u,"
                        " did not resolve to any address\n", this, off);
        return nullptr;
    }


    const Value* Pointer::carefulDeref(bool wide,
                                       const void* &dataStart,
                                       const void* &dataEnd) const noexcept
    {
        uint32_t off = wide ? offset<true>() : offset<false>();
        if (off == 0)
            return nullptr;
        const Value *target = offsetby(this, -(ptrdiff_t)off);

        if (_usuallyFalse(isExternal())) {
            slice destination;
            tie(target, destination) = Doc::resolvePointerFromWithRange(this, target);
            if (_usuallyFalse(!target)) {
                // Either invalid extern ref, or a legacy pointer without an 'extern' flag
                if (wide)
                    return nullptr;
                target = offsetby(this, -(ptrdiff_t)legacyOffset<false>());
                if (_usuallyFalse(target < dataStart) || _usuallyFalse(target >= dataEnd))
                    return nullptr;
                dataEnd = this;
            } else {
                assert_always((size_t(target) & 1) == 0);
                dataStart = destination.buf;
                dataEnd = destination.end();
            }
        } else {
            if (_usuallyFalse(target < dataStart) || _usuallyFalse(target >= dataEnd))
                return nullptr;
            dataEnd = this;
        }

        if (_usuallyFalse(target->isPointer()))
            return target->_asPointer()->carefulDeref(true, dataStart, dataEnd);
        else
            return target;
    }


    bool Pointer::validate(bool wide, const void *dataStart,
                           bool checkSharedKeyExists) const noexcept{
        const void *dataEnd = this;
        const Value *target = carefulDeref(wide, dataStart, dataEnd);
        return target && target->validate(dataStart, dataEnd, checkSharedKeyExists);
    }


} } }
