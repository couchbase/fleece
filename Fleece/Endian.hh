//
// Endian.hh
//
// Copyright (c) 2015 Couchbase, Inc All rights reserved.
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

#pragma once
extern "C" {
    #define __ENDIAN_SAFE
    #include "forestdb_endian.h"
}

namespace fleece {

#ifndef _LITTLE_ENDIAN
    // convert to little endian
    #define _encLittle64(v) bitswap64(v)
    #define _decLittle64(v) bitswap64(v)
    #define _encLittle32(v) bitswap32(v)
    #define _decLittle32(v) bitswap32(v)
    #define _encLittle16(v) bitswap16(v)
    #define _decLittle16(v) bitswap16(v)
#else
    #define _encLittle64(v) (v)
    #define _decLittle64(v) (v)
    #define _encLittle32(v) (v)
    #define _decLittle32(v) (v)
    #define _encLittle16(v) (v)
    #define _decLittle16(v) (v)
#endif


    namespace internal {
        inline uint16_t swapLittle(uint16_t n)  {return (uint16_t)_encLittle16(n);}
        inline uint16_t swapBig(uint16_t n)     {return (uint16_t)_enc16(n);}
        inline uint32_t swapLittle(uint32_t n)  {return _encLittle32(n);}
        inline uint32_t swapBig(uint32_t n)     {return _enc32(n);}
        inline uint64_t swapLittle(uint64_t n)  {return _encLittle64(n);}
        inline uint64_t swapBig(uint64_t n)     {return _enc64(n);}

        template <class INT, INT SWAP(INT)>
        class endian {
        public:
            endian()                :endian(0) { }
            endian(INT o)           :_swapped(SWAP(o)) { }
            operator INT () const   {return SWAP(_swapped);}
        private:
            INT _swapped;
        };


        template <class INT, INT SWAP(INT)>
        class endian_unaligned {
        public:
            endian_unaligned()
            :endian_unaligned(0)
            { }
            endian_unaligned(INT o) {
                o = SWAP(o);
                memcpy(_bytes, &o, sizeof(o));
            }
            operator INT () const {
                INT o;
                memcpy(&o, _bytes, sizeof(o));
                return SWAP(o);
            }
        private:
            uint8_t _bytes[sizeof(INT)];
        };

        inline void swapLittle(uint16_t &n) {n = _encLittle16(n);}
        inline void swapBig(uint16_t &n)    {n = (uint16_t)_enc16(n);}
        inline void swapLittle(uint32_t &n) {n = _encLittle32(n);}
        inline void swapBig(uint32_t &n)    {n = _enc32(n);}
        inline void swapLittle(uint64_t &n) {n = _encLittle64(n);}
        inline void swapBig(uint64_t &n)    {n = _enc64(n);}


        // Template for opaque endian floating-point value.
        template <typename FLT, typename RAW, void SWAP(RAW&)>
        struct endianFP {
            endianFP() {}
            endianFP(FLT f)           {*this = f;}
            endianFP(RAW raw)         {_swapped.asRaw = raw;}
            endianFP& operator= (FLT f) {
                _swapped.asNumber = f;
                SWAP(_swapped.asRaw);
                return *this;
            }
            operator FLT() const {
                swapped unswap = _swapped;
                SWAP(unswap.asRaw);
                return unswap.asNumber;
            }
            RAW raw() {return _swapped.asRaw;}
        protected:
            union swapped {
                FLT asNumber;
                RAW asRaw;
            };
            swapped _swapped;
        };
    }

    using uint16_le = internal::endian<uint16_t, internal::swapLittle>;
    using uint32_le = internal::endian<uint32_t, internal::swapLittle>;
    using uint64_le = internal::endian<uint64_t, internal::swapLittle>;
    using uint32_le_unaligned = internal::endian_unaligned<uint32_t, internal::swapLittle>;

    // Floating-point types whose storage is reliably big- or little-endian,
    // but which can be read and written as native numbers.

    typedef internal::endianFP<float,  uint32_t, internal::swapLittle> littleEndianFloat;
    typedef internal::endianFP<float,  uint32_t, internal::swapBig>    bigEndianFloat;
    typedef internal::endianFP<double, uint64_t, internal::swapLittle> littleEndianDouble;
    typedef internal::endianFP<double, uint64_t, internal::swapBig>    bigEndianDouble;

}
