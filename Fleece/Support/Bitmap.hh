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
#ifdef __clang__
#include <algorithm>    // for std::__pop_count
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
#ifdef __clang__
            return std::__pop_count(bits);
#else
            return __builtin_popcountl(bits);
#endif
        }

        static Rep mask(unsigned bitNo)         {return Rep(1) << bitNo;}

        Rep _bits;
    };

    template <class Rep>
    inline Bitmap<Rep> asBitmap(Rep bits)     {return Bitmap<Rep>(bits);}

}
