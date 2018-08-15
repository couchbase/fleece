//
//  Bitmap.h
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

#pragma once

#ifndef _MSC_VER
extern "C" {
    // Clang & GCC builtin functions:
    extern int __builtin_popcount(unsigned int) noexcept;
    extern int __builtin_popcountl(unsigned long) noexcept;
    extern int __builtin_popcountll(unsigned long long) noexcept;
}
#else
#include <intrin.h>
#include <mutex>
#include <bitset>
#include <array>

static std::once_flag once;

// https://graphics.stanford.edu/~seander/bithacks.html
template<typename T>
static T popcount_c(T v)
{
    v = v - ((v >> 1) & (T)~(T)0/3);                           // temp
    v = (v & (T)~(T)0/15*3) + ((v >> 2) & (T)~(T)0/15*3);      // temp
    v = (v + (v >> 4)) & (T)~(T)0/255*15;                      // temp
    return (T)(v * ((T)~(T)0/255)) >> (sizeof(T) - 1) * CHAR_BIT; // count
}


static bool can_popcnt = false;
static void detect_popcnt()
{
    // To determine hardware support for the popcnt instruction, call the __cpuid 
    // intrinsic with InfoType=0x00000001 and check bit 23 of CPUInfo[2] (ECX). 
    // This bit is 1 if the instruction is supported, and 0 otherwise. If you run 
    // code that uses this intrinsic on hardware that does not support the popcnt 
    // instruction, the results are unpredictable.
    // https://msdn.microsoft.com/en-us/library/bb385231.aspx
    #ifndef _M_ARM
    std::array<int, 4> cpui;
    __cpuid(cpui.data(), 0);
    int nIDs = cpui[0];
    if(nIDs >= 1) {
        __cpuidex(cpui.data(), 1, 0);
        std::bitset<32> ecx = cpui[2];
        can_popcnt = ecx[23];
    }
    #endif
}

extern "C" {
    static inline int __builtin_popcount(unsigned int v) noexcept
    {
#ifndef _M_ARM
        std::call_once(once, ::detect_popcnt);
        return can_popcnt ? (int)__popcnt(v) : popcount_c(v);
#else
        return popcount_c(v);
#endif
    }

    static inline int __builtin_popcountl(unsigned long v) noexcept
    {
        
#ifndef _M_ARM
        std::call_once(once, ::detect_popcnt);
        return can_popcnt ? (int)__popcnt(v) : popcount_c(v);
#else
        return popcount_c(v);
#endif
    }

    static inline int __builtin_popcountll(unsigned long long v) noexcept
    {
#ifdef _WIN64
        std::call_once(once, ::detect_popcnt);
        return can_popcnt ? (int)__popcnt64(v) : (int)popcount_c(v);
#else
        return (int)popcount_c(v);
#endif
    }
}
#endif

namespace fleece {

    template <class Rep>
    class Bitmap {
    public:
        Bitmap()                                :Bitmap(0) { }
        explicit Bitmap(Rep bits)               :_bits(bits) { }
        explicit operator Rep () const          {return _bits;}

        static constexpr unsigned capacity = sizeof(Rep) * 8;

        unsigned bitCount() const               {return popcount(_bits);}
        bool empty() const                      {return _bits == 0;}

        bool containsBit(unsigned bitNo) const    {return (_bits & mask(bitNo)) != 0;}
        unsigned indexOfBit(unsigned bitNo) const {return popcount( _bits & (mask(bitNo) - 1) );}

        void addBit(unsigned bitNo)             {_bits |= mask(bitNo);}
        void removeBit(unsigned bitNo)          {_bits &= ~mask(bitNo);}

    private:
        static unsigned popcount(Rep bits) {
            if (sizeof(Rep) <= sizeof(int))
                return __builtin_popcount(bits);
            else if (sizeof(Rep) <= sizeof(long))
                return __builtin_popcountl(bits);
            else
                return __builtin_popcountll(bits);
        }

        static Rep mask(unsigned bitNo)         {return Rep(1) << bitNo;}

        Rep _bits;
    };

    template <class Rep>
    inline Bitmap<Rep> asBitmap(Rep bits)     {return Bitmap<Rep>(bits);}

}
