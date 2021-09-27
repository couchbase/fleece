//
//  Bitmap.h
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
#include <type_traits>

#ifndef _MSC_VER
extern "C" {
    // Clang & GCC builtin functions for hardware-accelerated popcount:
    extern int __builtin_popcount(unsigned int);
    extern int __builtin_popcountl(unsigned long);
    extern int __builtin_popcountll(unsigned long long);
}
#endif


namespace fleece {

    /** Returns the number of 1 bits in the integer `bits`. */
    template<class INT, typename std::enable_if<std::is_integral<INT>::value, INT>::type = 0>
    INT popcount(INT bits) {
#ifdef _MSC_VER
        extern int _popcount(unsigned int) noexcept;
        extern int _popcountl(unsigned long) noexcept;
        extern int _popcountll(unsigned long long) noexcept;
        if (sizeof(INT) <= sizeof(int))
            return _popcount(bits);
        else if (sizeof(INT) <= sizeof(long))
            return _popcountl(bits);
        else
            return _popcountll(bits);
#else
        if (sizeof(INT) <= sizeof(int))
            return __builtin_popcount(bits);
        else if (sizeof(INT) <= sizeof(long))
            return __builtin_popcountl(bits);
        else
            return __builtin_popcountll(bits);
#endif
    }


    /** A compact fixed-size array of bits. It's backed by an integer type `Rep`,
        so the available capacities are 8, 16, 32, 64 bits. */
    template <class Rep>
    class Bitmap {
    public:
        Bitmap()                                :Bitmap(0) { }
        explicit Bitmap(Rep bits)               :_bits(bits) { }
        explicit operator Rep () const          {return _bits;}

        /** Number of bits in a Bitmap */
        static constexpr unsigned capacity = sizeof(Rep) * 8;

        /** Total number of 1 bits */
        unsigned bitCount() const               {return popcount(_bits);}

        /** True if no bits are 1 */
        bool empty() const                      {return _bits == 0;}

        /** True if the bit at index `bitNo` (with 0 being the LSB) is 1. */
        bool containsBit(unsigned bitNo) const    {return (_bits & mask(bitNo)) != 0;}

        /** The number of 1 bits before `bitNo`. */
        unsigned indexOfBit(unsigned bitNo) const {return popcount( _bits & (mask(bitNo) - 1) );}

        /** Sets bit `bitNo` to 1. */
        void addBit(unsigned bitNo)             {_bits |= mask(bitNo);}

        /** Sets bit `bitNo` to 0. */
        void removeBit(unsigned bitNo)          {_bits &= ~mask(bitNo);}

    private:
        static Rep mask(unsigned bitNo)         {return Rep(1) << bitNo;}

        Rep _bits;
    };

    /** Utility function for constructing a Bitmap from an integer. */
    template <class Rep>
    inline Bitmap<Rep> asBitmap(Rep bits)     {return Bitmap<Rep>(bits);}

    template <class Rep>
    constexpr unsigned Bitmap<Rep>::capacity;
}
