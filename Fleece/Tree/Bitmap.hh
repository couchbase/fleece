//
//  Bitmap.h
//  Fleece
//
//  Created by Jens Alfke on 4/23/18.
//  Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include <algorithm>    // for __pop_count
#include <vector>

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


    template <class Rep, class Val>
    class BitmapVector {
    public:
        BitmapVector() { }
        BitmapVector(unsigned capacity)     {_values.reserve(capacity);}

        Val* get(unsigned bitNo) {
            return _bitmap.containsBit(bitNo) ? &_values[_bitmap.indexOfBit(bitNo)] : nullptr;
        }

        const Val* get(unsigned bitNo) const {
            return const_cast<BitmapVector*>(this)->get(bitNo);
        }

        unsigned size() const {
            return (unsigned)_values.size();
        }

        bool empty() const {
            return _bitmap.empty();
        }

        bool contains(unsigned bitNo) const {
            return _bitmap.containsBit(bitNo);
        }

        template <class T>
        Val* put(unsigned bitNo, const T &val) {
            auto i = &_values[_bitmap.indexOfBit(bitNo)];
            if (contains(bitNo)) {
                *i = val;
            } else {
                _bitmap.addBit(bitNo);
                _values.insert(i, val);
            }
            return i;
        }

        template <class... _Args>
        Val* emplace(unsigned bitNo, _Args&&... __args) {
            auto i = &_values[_bitmap.indexOfBit(bitNo)];
            if (contains(bitNo)) {
                *i = Val(std::forward<_Args>(__args)...);
            } else {
                _bitmap.addBit(bitNo);
                _values.emplace(i, std::forward<_Args>(__args)...);
            }
            return i;
        }

        void erase(unsigned bitNo) {
            if (contains(bitNo)) {
                _values.erase(&_values[_bitmap.indexOfBit(bitNo)]);
                _bitmap.removeBit(bitNo);
            }
        }

        using iterator = typename std::vector<Val>::iterator;
        using const_iterator = typename std::vector<Val>::const_iterator;

        iterator begin()        {return _values.begin();}
        iterator end()          {return _values.begin();}
        const_iterator begin() const  {return _values.begin();}
        const_iterator end() const    {return _values.begin();}

    private:
        Bitmap<Rep> _bitmap;
        std::vector<Val> _values;
    };
}
