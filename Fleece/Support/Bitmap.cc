//
// Bitmap.cc
//
// Copyright © 2018 Couchbase. All rights reserved.
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

#include "Bitmap.hh"

#ifdef _MSC_VER

#include <mutex>
#include <bitset>
#include <array>
#include <intrin.h>


namespace fleece {

    /** Portable C implementation of popcount, courtesy of
        https://graphics.stanford.edu/~seander/bithacks.html */
    template<typename T>
    static inline int popcount_c(T v)
    {
        v = v - ((v >> 1) & (T)~(T)0/3);                           // temp
        v = (v & (T)~(T)0/15*3) + ((v >> 2) & (T)~(T)0/15*3);      // temp
        v = (v + (v >> 4)) & (T)~(T)0/255*15;                      // temp
        v = (T)(v * ((T)~(T)0/255)) >> (sizeof(T) - 1) * CHAR_BIT; // count
        return (int)v;
    }


#ifndef _M_ARM
    static bool _can_popcnt = false;
    static void detect_popcnt()
    {
        // To determine hardware support for the popcnt instruction, call the __cpuid
        // intrinsic with InfoType=0x00000001 and check bit 23 of CPUInfo[2] (ECX).
        // This bit is 1 if the instruction is supported, and 0 otherwise. If you run
        // code that uses this intrinsic on hardware that does not support the popcnt
        // instruction, the results are unpredictable.
        // https://msdn.microsoft.com/en-us/library/bb385231.aspx
        std::array<int, 4> cpui;
        __cpuid(cpui.data(), 0);
        int nIDs = cpui[0];
        if(nIDs >= 1) {
            __cpuidex(cpui.data(), 1, 0);
            std::bitset<32> ecx = cpui[2];
            _can_popcnt = ecx[23];
        }
    }

    static inline bool can_popcnt() {
        static std::once_flag once;
        if (!_can_popcnt)
            std::call_once(once, detect_popcnt);
        return _can_popcnt;
    }
#endif

    int _popcount(unsigned int v) noexcept {
#ifndef _M_ARM
        return can_popcnt() ? __popcnt(v) : popcount_c(v);
#else
        return popcount_c(v);
#endif;
    }

    int _popcountl(unsigned long v) noexcept {
#ifndef _M_ARM
        return can_popcnt() ? __popcnt(v) : popcount_c(v);
#else
        return popcount_c(v);
#endif;
    }

    int _popcountll(unsigned long long v) noexcept {
#ifdef _WIN64
        return can_popcnt() ? (int)__popcnt64(v) : popcount_c(v);
#else
        return popcount_c(v);
#endif
    }
    
}

#endif // _MSC_VER
