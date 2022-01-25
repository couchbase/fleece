//
// SmallVector.hh
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
#include "SmallVectorBase.hh"

namespace fleece {

    /// Similar to std::vector but optimized for small sizes. The first N items are stored inside the
    /// object itself. This makes the object larger, but avoids heap allocation. Once it grows larger
    /// than n items, it will switch to using a heap block like a vector.
    ///
    /// \warning This class cuts a few corners, which slightly limits what types you can use it with:
    ///         * Item constructors should not throw exceptions, or smallVector may not clean up properly.
    ///         * When the item array grows, the items are moved with memcpy, not a move constructor.
    ///           This is more efficient but will break those rare classes that need special attention
    ///           when moved. (Other optimized vectors like Folly's have a similar requirement.)
    template <class T, size_t N>
    class smallVector : public smallVectorBase {
    public:
        static constexpr size_t max_size = (1ul<<31) - 1;

        smallVector() noexcept                          :smallVectorBase(N) { }
        smallVector(size_t size)                        :smallVector() {resize(size);}

        template <size_t M>
        smallVector(const smallVector<T,M> &sv)         :smallVector(sv.begin(), sv.end()) { }
        smallVector(const smallVector &sv)              :smallVector(sv.begin(), sv.end()) { }

        smallVector(smallVector &&sv) noexcept          :smallVectorBase() {_moveFrom(std::move(sv), sizeof(*this));}

        template <class ITER>
        smallVector(ITER b, ITER e)
        :smallVector()
        {
            setCapacity(e - b);
            while (b != e)
                heedlessPushBack(*b++);
        }

        ~smallVector() {
            if (_size > 0) {
                auto item = begin();
                for (decltype(_size) i = 0; i < _size; ++i)
                    (item++)->T::~T();
            }
        }

        smallVector& operator=(smallVector &&sv) noexcept {
            clear();
            _moveFrom(std::move(sv), sizeof(*this));
            return *this;
        }

        smallVector& operator=(const smallVector &sv) noexcept {
            return operator=<N>(sv);
        }

        template <size_t M>
        smallVector& operator=(const smallVector<T,M> &sv) {
            erase(begin(), end());
            setCapacity(sv.size());
            for (const auto &val : sv)
                heedlessPushBack(val);
            return *this;
        }

        template <size_t M>
        bool operator== (const smallVector<T,M> &v) const
#if defined(__clang__) || !defined(__GNUC__)
        FLPURE  /* for some reason GCC does not like this attribute here*/
#endif
        {
            if (_size != v._size)
                return false;
            auto vi = v.begin();
            for (auto &e : *this) {
                if (!(e == *vi))
                    return false;
                ++vi;
            }
            return true;
        }

        template <size_t M>
        bool operator!= (const smallVector<T,M> &v) const
#if defined(__clang__) || !defined(__GNUC__)
        FLPURE  /* for some reason GCC does not like this attribute here*/
#endif
        {
            return !( *this == v);
        }

        void clear()                                    {shrinkTo(0);}
        void reserve(size_t cap)                        {if (cap>_capacity) setCapacity(cap);}

        const T& get(size_t i) const FLPURE {
            assert_precondition(i < _size);
            return _get(i);
        }

        T& get(size_t i) FLPURE {
            assert_precondition(i < _size);
            return _get(i);
        }

        const T& operator[] (size_t i) const FLPURE     {return get(i);}
        T& operator[] (size_t i) FLPURE                 {return get(i);}
        const T& back() const FLPURE                    {return get(_size - 1);}
        T& back() FLPURE                                {return get(_size - 1);}

        using iterator = T*;
        using const_iterator = const T*;

        iterator begin() FLPURE                         {return &_get(0);}
        iterator end() FLPURE                           {return &_get(_size);}
        const_iterator begin() const FLPURE             {return &_get(0);}
        const_iterator end() const FLPURE               {return &_get(_size);}

        T& push_back(const T& t)                        {return * new(grow()) T(t);}
        T& push_back(T&& t)                             {return * new(grow()) T(std::move(t));}

        void pop_back()                                 {get(_size - 1).~T(); --_size;}

        template <class... Args>
        T& emplace_back(Args&&... args) {
            return * new(grow()) T(std::forward<Args>(args)...);
        }

        void insert(iterator where, T item) {
            assert_precondition(begin() <= where && where <= end());
            void *dst = _insert(where, 1, kItemSize);
            *(T*)dst = std::move(item);
        }

        template <class ITER>
        void insert(iterator where, ITER b, ITER e) {
            assert_precondition(begin() <= where && where <= end());
            auto n = e - b;
            assert_precondition(n >= 0 && n <= max_size);
            T *dst = (T*)_insert(where, uint32_t(n), kItemSize);
            while (b != e)
                *dst++ = *b++;
        }

        void erase(iterator first) {
            erase(first, first+1);
        }

        void erase(iterator first, iterator last) {
            assert_precondition(begin() <= first && first <= last && last <= end());
            if (first == last)
                return;
            for (auto i = first; i != last; ++i)
                i->T::~T();                 // destruct removed items
            _moveItems(first, last, end());
            _size -= last - first;
        }

        void resize(size_t sz) {
            auto oldSize = _size;
            if (sz > oldSize) {
                uint32_t newSize = rangeCheck(sz);
                auto i = (iterator)_growTo(newSize, kItemSize);
                for (; oldSize < newSize; ++oldSize)
                    (void) new (i++) T();       // default-construct new items
            } else {
                shrinkTo(sz);
            }
        }

        void setCapacity(size_t cap) {
            if (cap != _capacity) {
                if (cap > N)
                    _embiggen(cap, kItemSize);
                else if (_capacity != N)
                    _emsmallen(N, kItemSize);
            }
        }

        // Appends space for a new item but does not construct it.
        // Returns a pointer to the space where the new item would go; the caller needs to
        // put a valid value there or Bad Things will happen later.
        // (This method was added for the user of Encoder and is probably not too useful elsewhere.)
        void* push_back_new()                   {return grow();}

    private:
        T& _get(size_t i) FLPURE                {return ((T*)_begin())[i];}
        const T& _get(size_t i) const FLPURE    {return ((T*)_begin())[i];}

        T* grow()                               {return (T*)_growTo(_size + 1, kItemSize);}
        T* heedlessGrow()                       {assert(_size < _capacity); return &_get(_size++);}
        T& heedlessPushBack(const T& t)         {return * new(heedlessGrow()) T(t);}

        void shrinkTo(size_t sz) {
            if (sz < _size) {
                auto item = end();
                for (auto i = sz; i < _size; ++i)
                    (--item)->T::~T();                 // destruct removed items
                _size = (uint32_t)sz;

                if (_isBig && sz <= N)
                    _emsmallen(N, kItemSize);
            }
        }


        // The size of one vector item, taking into account alignment.
        static constexpr size_t kItemSize = ((sizeof(T) + alignof(T) - 1) / alignof(T)) * alignof(T);

        static constexpr size_t kInlineCap = std::max(N * kItemSize, kBaseInlineCap);

        // Enough extra space to hold an inline array T[N].
        uint8_t _padding[kInlineCap - kBaseInlineCap];
    };

}
