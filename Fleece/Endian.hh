//
//  Endian.h
//  Fleece
//
//  Created by Jens Alfke on 1/29/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#pragma once
extern "C" {
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
        inline void _swapLittle(uint32_t &n) {n = _encLittle32(n);}
        inline void _swapBig(uint32_t &n)    {n = _enc32(n);}
        inline void _swapLittle(uint64_t &n) {n = _encLittle64(n);}
        inline void _swapBig(uint64_t &n)    {n = _enc64(n);}


        // Template for opaque endian floating-point value.
        template <typename FLT, typename RAW, void SWAP(RAW&)>
        struct endian {
            endian() {}
            endian(FLT f)           {*this = f;}
            endian(uint32_t raw)    {_swapped.asRaw = raw;}
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
