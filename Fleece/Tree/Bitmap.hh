//
//  Bitmap.h
//  Fleece
//
//  Created by Jens Alfke on 4/23/18.
//  Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include <algorithm>

namespace fleece {

    template <class Rep>
    class Bitmap {
    public:
        Bitmap()                                :Bitmap(0) { }
        explicit Bitmap(Rep bits)               :_bits(bits) { }
        explicit operator Rep () const          {return _bits;}

        static unsigned capacity()              {return sizeof(Rep) / 8;}
        unsigned bitCount() const               {return popcount(_bits);}
        bool empty() const                      {return _bits == 0;}

        bool containsBit(unsigned bitNo) const    {return (_bits & mask(bitNo)) != 0;}
        unsigned indexOfBit(unsigned bitNo) const {return popcount( _bits & (mask(bitNo) - 1) );}

        void addBit(unsigned bitNo)             {_bits |= mask(bitNo);}
        void removeBit(unsigned bitNo)          {_bits &= ~mask(bitNo);}

    private:
        static unsigned popcount(Rep bits)      {return std::__pop_count(bits);}
        static Rep mask(unsigned bitNo)         {return Rep(1) << bitNo;}

        Rep _bits;
    };

    template <class Rep>
    inline Bitmap<Rep> asBitmap(Rep bits)     {return Bitmap<Rep>(bits);}
}
