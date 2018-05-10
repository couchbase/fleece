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


    // Little-endian 32-bit integer
    class uint32_le {
    public:
        uint32_le(uint32_t o) {
            o = _encLittle32(o);
            memcpy(bytes, &o, sizeof(bytes));
        }
        operator uint32_t () const {
            uint32_t o;
            memcpy(&o, bytes, sizeof(o));
            return _decLittle32(o);
        }
    private:
        uint8_t bytes[sizeof(uint32_t)];
    };


    // Little-endian 64-bit integer
    class uint64_le {
    public:
        uint64_le(uint64_t o) {
            o = _encLittle64(o);
            memcpy(bytes, &o, sizeof(bytes));
        }
        operator uint64_t () const {
            uint64_t o;
            memcpy(&o, bytes, sizeof(o));
            return _decLittle64(o);
        }
    private:
        uint8_t bytes[sizeof(uint64_t)];
    };


    namespace internal {
        inline void _swapLittle(uint32_t &n) {n = _encLittle32(n);}
        inline void _swapBig(uint32_t &n)    {n = _enc32(n);}
        inline void _swapLittle(uint64_t &n) {n = _encLittle64(n);}
        inline void _swapBig(uint64_t &n)    {n = _enc64(n);}


        // Template for opaque endian floating-point value.
        template <typename FLT, typename RAW, void SWAP(RAW&)>
        struct endian {
            endian() {}
            endian(FLT f)           {*this = f;}
            endian(RAW raw)         {_swapped.asRaw = raw;}
            endian& operator= (FLT f) {
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

    // Floating-point types whose storage is reliably big- or little-endian,
    // but which can be read and written as native numbers.

    typedef internal::endian<float,  uint32_t, internal::_swapLittle> littleEndianFloat;
    typedef internal::endian<float,  uint32_t, internal::_swapBig>    bigEndianFloat;
    typedef internal::endian<double, uint64_t, internal::_swapLittle> littleEndianDouble;
    typedef internal::endian<double, uint64_t, internal::_swapBig>    bigEndianDouble;

}
