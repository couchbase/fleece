//
//  NanTagged.hh
//  Fleece
//
//  Created by Jens Alfke on 2/24/20.
//  Copyright © 2020 Couchbase. All rights reserved.
//

#pragma once
#include "PlatformCompat.hh"
#include <cmath>
#include <limits>

namespace fleece {

    /** A self-describing 8-byte value that can store a double, a pointer, or six bytes of data,
        and can identify which it's holding at any time.
        Uses the so-called "NaN tagging" trick used by several dynamic language runtimes. */
    template <class T>
    class NanTagged {
    public:
        /** How many bytes of inline data I can hold. */
        static constexpr size_t kInlineCapacity = 6;

        NanTagged(double d =0.0)                    :_asDouble(d) { }
        NanTagged(const T *ptr)                     {setPointer(ptr);}
        NanTagged(std::initializer_list<uint8_t> b) {setInline(b);}

        NanTagged(const NanTagged &n)               :_bits(n._bits) { }
        NanTagged& operator=(const NanTagged &n)    {_bits = n._bits; return *this;}

        bool isDouble() const noexcept FLPURE       {return (_bits & kQNaNBits) != kQNaNBits;}
        bool isPointer() const noexcept FLPURE      {return signBitSet() && !isDouble();}
        bool isInline() const noexcept FLPURE       {return !signBitSet() && !isDouble();}

        double asDouble() const noexcept FLPURE     {return isDouble() ? doubleValue() : 0.0;}
        const T* asPointer() const noexcept FLPURE  {return isPointer() ? pointerValue() : nullptr;}
        slice asInline() const noexcept FLPURE      {return isInline() ? inlineBytes() : slice();}

        double doubleValue() const noexcept FLPURE  {return _asDouble;}
        const T* pointerValue() const noexcept FLPURE  {return (T*)(_bits & ~(kSignBit | kQNaNBits));}

        uint64_t inlineBits() const noexcept FLPURE {return _bits & ~(kQNaNBits);}
        slice inlineBytes() const noexcept FLPURE    {return {&_bytes[kInlineOffset], kInlineCapacity};}
        const T* inlinePointer() const noexcept FLPURE {return (const T*) &_bytes[kInlineOffset];}
        T* inlinePointer() noexcept                 {return (T*) &_bytes[kInlineOffset];}

        void setDouble(double d) noexcept {
            if (_usuallyFalse(isnan(d)))
                d = std::numeric_limits<double>::infinity();
            _asDouble = d;
        }

        void setPointer(const T *p) noexcept {
            _bits = (uint64_t)p | kQNaNBits | kSignBit;
        }

        void setInline(slice s) noexcept {
            assert(s.size <= kInlineCapacity);
            _bits = kQNaNBits;
            memcpy(&_bytes[0], s.buf, s.size);
        }

        void setInline(std::initializer_list<uint8_t> inlineBytes) {
            setInline({inlineBytes.begin(), inlineBytes.size()});
        }

    private:
        bool signBitSet() const noexcept FLPURE      {return (_bits & kSignBit) != 0;}

        static constexpr uint64_t kSignBit  = 0x8000000000000000; // Sign bit of a double
        static constexpr uint64_t kQNaNBits = 0x7ffc000000000000; // Bits set in a 'quiet' NaN

        // In little-endian the 51 free bits are at the start, in big-endian at the end.
#ifdef __LITTLE_ENDIAN__
        static constexpr auto kInlineOffset = 0;
#else
        static constexpr auto kInlineOffset = 2;
#endif

        union {
            double              _asDouble;
            uint64_t            _bits;
            uint8_t             _bytes[sizeof(double)];
        };
    };

}
