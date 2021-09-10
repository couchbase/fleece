//
// Endian.hh
//
// Copyright 2015-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "PlatformCompat.hh"
#include "endianness.h"

namespace fleece { namespace endian {

#ifdef __BIG_ENDIAN__
    FLCONST static inline uint64_t enc64(uint64_t v) noexcept {return v;}
    FLCONST static inline uint64_t dec64(uint64_t v) noexcept {return v;}
    FLCONST static inline uint32_t enc32(uint32_t v) noexcept {return v;}
    FLCONST static inline uint32_t dec32(uint32_t v) noexcept {return v;}
    FLCONST static inline uint16_t enc16(uint16_t v) noexcept {return v;}
    FLCONST static inline uint16_t dec16(uint16_t v) noexcept {return v;}
#else
    // convert to big endian
    FLCONST static inline uint64_t enc64(uint64_t v) noexcept {return bswap64(v);}
    FLCONST static inline uint64_t dec64(uint64_t v) noexcept {return bswap64(v);}
    FLCONST static inline uint32_t enc32(uint32_t v) noexcept {return bswap32(v);}
    FLCONST static inline uint32_t dec32(uint32_t v) noexcept {return bswap32(v);}
    FLCONST static inline uint16_t enc16(uint16_t v) noexcept {return bswap16(v);}
    FLCONST static inline uint16_t dec16(uint16_t v) noexcept {return bswap16(v);}
#endif

#ifdef __LITTLE_ENDIAN__
    FLCONST static inline uint64_t encLittle64(uint64_t v) noexcept {return v;}
    FLCONST static inline uint64_t decLittle64(uint64_t v) noexcept {return v;}
    FLCONST static inline uint32_t encLittle32(uint32_t v) noexcept {return v;}
    FLCONST static inline uint32_t decLittle32(uint32_t v) noexcept {return v;}
    FLCONST static inline uint16_t encLittle16(uint16_t v) noexcept {return v;}
    FLCONST static inline uint16_t decLittle16(uint16_t v) noexcept {return v;}
#else
    // convert to little endian
    FLCONST static inline uint64_t encLittle64(uint64_t v) noexcept {return bswap64(v);}
    FLCONST static inline uint64_t decLittle64(uint64_t v) noexcept {return bswap64(v);}
    FLCONST static inline uint32_t encLittle32(uint32_t v) noexcept {return bswap32(v);}
    FLCONST static inline uint32_t decLittle32(uint32_t v) noexcept {return bswap32(v);}
    FLCONST static inline uint16_t encLittle16(uint16_t v) noexcept {return bswap16(v);}
    FLCONST static inline uint16_t decLittle16(uint16_t v) noexcept {return bswap16(v);}
#endif


    namespace internal {
        FLCONST inline uint16_t swapLittle(uint16_t n) noexcept {return (uint16_t)encLittle16(n);}
        FLCONST inline uint16_t swapBig(uint16_t n)    noexcept {return (uint16_t)enc16(n);}
        FLCONST inline uint32_t swapLittle(uint32_t n) noexcept {return encLittle32(n);}
        FLCONST inline uint32_t swapBig(uint32_t n)    noexcept {return enc32(n);}
        FLCONST inline uint64_t swapLittle(uint64_t n) noexcept {return encLittle64(n);}
        FLCONST inline uint64_t swapBig(uint64_t n)    noexcept {return enc64(n);}

        template <class INT, INT SWAP(INT)>
        class endian {
        public:
            endian() noexcept                       :endian(0) { }
            endian(INT o) noexcept                  :_swapped(SWAP(o)) { }
            FLPURE operator INT () const noexcept   {return SWAP(_swapped);}
        private:
            INT _swapped;
        };


        template <class INT, INT SWAP(INT)>
        class endian_unaligned {
        public:
            endian_unaligned() noexcept
            :endian_unaligned(0)
            { }
            endian_unaligned(INT o) noexcept {
                o = SWAP(o);
                memcpy(_bytes, &o, sizeof(o));
            }
            FLPURE operator INT () const noexcept {
                INT o = 0;
                memcpy(&o, _bytes, sizeof(o));
                return SWAP(o);
            }
        private:
            uint8_t _bytes[sizeof(INT)];
        };

        inline void swapLittle(uint16_t &n) noexcept {n = encLittle16(n);}
        inline void swapBig(uint16_t &n)    noexcept {n = (uint16_t)enc16(n);}
        inline void swapLittle(uint32_t &n) noexcept {n = encLittle32(n);}
        inline void swapBig(uint32_t &n)    noexcept {n = enc32(n);}
        inline void swapLittle(uint64_t &n) noexcept {n = encLittle64(n);}
        inline void swapBig(uint64_t &n)    noexcept {n = enc64(n);}


        // Template for opaque endian floating-point value.
        template <typename FLT, typename RAW, void SWAP(RAW&)>
        struct endianFP {
            endianFP() noexcept                 {}
            endianFP(FLT f) noexcept            {*this = f;}
            endianFP(RAW raw) noexcept          {_swapped.asRaw = raw;}

            endianFP& operator= (FLT f) noexcept {
                _swapped.asNumber = f;
                SWAP(_swapped.asRaw);
                return *this;
            }
            FLPURE operator FLT() const noexcept {
                swapped unswap = _swapped;
                SWAP(unswap.asRaw);
                return unswap.asNumber;
            }
            FLPURE RAW raw() const noexcept {return _swapped.asRaw;}
        protected:
            union swapped {
                FLT asNumber;
                RAW asRaw;
            };
            swapped _swapped;
        };
    }

    // Integer types whose storage is always little-endian,
    // but which can be used like native ints:
    using uint16_le = internal::endian<uint16_t, internal::swapLittle>;
    using uint32_le = internal::endian<uint32_t, internal::swapLittle>;
    using uint64_le = internal::endian<uint64_t, internal::swapLittle>;

    // Little-endian uint32 whose storage is byte-aligned (not to a 4-byte boundary.)
    // This is somewhat slower to access but allows more compact structs.
    using uint32_le_unaligned = internal::endian_unaligned<uint32_t, internal::swapLittle>;

    // Floating-point types whose storage is always big- or little-endian,
    // but which can be used like native types:
    using littleEndianFloat  = internal::endianFP<float,  uint32_t, internal::swapLittle>;
    using bigEndianFloat     = internal::endianFP<float,  uint32_t, internal::swapBig>;
    using littleEndianDouble = internal::endianFP<double, uint64_t, internal::swapLittle>;
    using bigEndianDouble    = internal::endianFP<double, uint64_t, internal::swapBig>;

} }
