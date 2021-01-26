//
// SmallVector.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "PlatformCompat.hh"
#include <memory>
#include <stdexcept>
#include "betterassert.hh"

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
    class smallVector {
    public:
        smallVector() noexcept                          { }
        ~smallVector()                                  {clear(); assert(!_isBig);}

        smallVector(size_t size)                        :smallVector() {resize(size);}

        smallVector(const smallVector &sv)              {*this = sv;}
        smallVector(smallVector &&sv) noexcept          {*this = std::move(sv);}

        template <class ITER>
        smallVector(ITER b, ITER e) {
            setCapacity(e - b);
            while (b != e)
                heedless_push_back(*b++);
        }

        smallVector& operator=(const smallVector &sv) {
            clear();
            setCapacity(sv.size());
            for (const auto &val : sv)
                heedless_push_back(val);
            return *this;
        }

        smallVector& operator=(smallVector &&sv) noexcept {
            clear();
            _size = sv._size;
            _isBig = (_size > N);
            if (_isBig)
                new (&_variant) big_t(std::move(sv.big()));
            else
                new (&_variant) small_t(std::move(sv.small()));
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

        size_t size() const FLPURE                      {return _size;}
        bool empty() const FLPURE                       {return _size == 0;}
        void clear()                                    {shrinkTo(0);}
        void reserve(size_t cap)                        {if (cap>capacity()) setCapacity(cap);}

        uint32_t capacity() const FLPURE {
            if (_isBig)
                return big().capacity();
            else
                return N;
        }

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

        T& push_back(const T& t)                        {return * new(_grow()) T(t);}
        T& push_back(T&& t)                             {return * new(_grow()) T(std::move(t));}

        void* push_back()                               {return _grow();}
        void pop_back()                                 {get(_size - 1).~T(); --_size;}

        template <class... Args>
        T& emplace_back(Args&&... args) {
            return * new(_grow()) T(std::forward<Args>(args)...);
        }

        void insert(iterator where, T item) {
            assert_precondition(begin() <= where && where <= end());
            if (_usuallyFalse(_size >= capacity())) {
                size_t i = where - &_get(0);
                _grow();
                where = &_get(i);
            } else
                ++_size;
            memmove(where+1, where, (uint8_t*)end() - (uint8_t*)(where + 1));
            *where = std::move(item);
        }

        void erase(iterator first) {
            assert_precondition(begin() <= first && first < end());
            first->T::~T();                 // destruct removed item
            memmove(first, first + 1, (end() - first - 1) * kItemSize);
            --_size;
        }

        void erase(iterator first, iterator last) {
            assert_precondition(begin() <= first && first < last && last <= end());
            for (auto i = first; i < last; ++i)
                i->T::~T();                 // destruct removed items
            memmove(first, last, (end() - last) * kItemSize);
            _size -= last - first;
        }

        void resize(size_t sz) {
            if (sz > _size) {
                auto newSize = rangeCheck(sz);
                auto cap = capacity();
                if (_usuallyFalse(newSize > cap))
                    setCapacity(std::max((size_t)cap + cap/2, sz));
                auto s = _size;
                _size = newSize;
                auto i = &_get(s);
                for (; s < newSize; ++s)
                    (void) new (i++) T();       // default-construct new items
            } else {
                shrinkTo(sz);
            }
        }

        void setCapacity(size_t cap) {
            if (_usuallyFalse(cap < _size))
                throw std::logic_error("capacity smaller than size");
            if (cap <= N) {
                if (_isBig) {
                    big_t tempBig = std::move(big());
                    new (&_variant) small_t(tempBig.base(), _size);
                    _isBig = false;
                }
            } else {
                uint32_t newCap = rangeCheck(cap);
                if (_isBig) {
                    big().setCapacity(newCap, _size);
                } else {
                    big_t newBig(newCap, small().base(), _size);
                    new (&_variant) big_t(std::move(newBig));
                    _isBig = true;
                }
            }
        }

    private:
        static uint32_t rangeCheck(size_t n) {
            if (_usuallyFalse(n > UINT32_MAX))
                throw std::domain_error("smallVector size/capacity too large");
            return uint32_t(n);
        }

        T& _get(size_t i) FLPURE {
            T *base = _isBig ? big().base() : small().base();
            return base[i];
        }

        const T& _get(size_t i) const FLPURE {
            return const_cast<smallVector*>(this)->_get(i);
        }

        // Grow size by one and return new (unconstructed!) item
        T* _grow() {
            if (_usuallyFalse(_size >= capacity()))
                setCapacity(std::max((size_t)capacity() + capacity()/2, (size_t)_size + 1));
            return &_get(_size++);
        }

        // Same as `grow` but assumes there is capacity. Does not resize or (in release) check.
        T* _heedlessGrow() {
            assert(_size < capacity());
            return &_get(_size++);
        }

        T& heedless_push_back(const T& t) {
            return * new(_heedlessGrow()) T(t);
        }

        void shrinkTo(size_t sz) {
            if (sz < _size) {
                auto item = end();
                for (auto i = sz; i < _size; ++i)
                    (--item)->T::~T();                 // destruct removed items
                _size = (uint32_t)sz;

                if (_isBig && _size <= N)
                    setCapacity(N);
            }
        }


        //---- STORAGE:  This class uses a custom smart union similar to C++17's `variant`.


        // The size of one vector item, taking into account alignment.
        static constexpr size_t kItemSize = ((sizeof(T) + alignof(T) - 1) / alignof(T)) * alignof(T);


        // Small variant, with inline buffer
        class small_t {
        public:
            small_t() noexcept                          { }
            small_t(T *items, uint32_t size) noexcept   {memmove(_buffer, items, size * kItemSize);}
            T* base() noexcept FLPURE                   {return (T*)_buffer;}
            uint32_t capacity() const noexcept FLPURE   {return N;}

        private:
            alignas(T) uint8_t _buffer[N * kItemSize];
        };


        // Large variant, with heap-allocated buffer
        class big_t {
        public:
            big_t(uint32_t capacity) {
                setCapacity(capacity, 0);
            }

            big_t(uint32_t capacity, T *items, uint32_t size)
            :big_t(capacity)
            {
                ::memmove(_buffer.get(), items, size * kItemSize);
            }

            T* base() noexcept FLPURE                   {return (T*)_buffer.get();}
            uint32_t capacity() const noexcept FLPURE   {return _capacity;}

            void setCapacity(uint32_t newCapacity, uint32_t size) {
                assert_precondition(newCapacity >= size && _capacity >= size);
                if (newCapacity == _capacity)
                    return;
                //OPT: Would be more optimal to use `realloc`
                auto newBytes = std::make_unique<uint8_t[]>(newCapacity * kItemSize);
                if (size > 0)
                    ::memmove(newBytes.get(), _buffer.get(), size * kItemSize);
                _buffer = std::move(newBytes);
                _capacity = newCapacity;
            }

        private:
            std::unique_ptr<uint8_t[]>  _buffer;
            uint32_t                    _capacity = 0;
        };


        small_t& small() {assert(!_isBig); return *(small_t*)&_variant;}
        big_t&   big()   {assert(_isBig); return *(big_t*)&_variant;}
        const small_t& small() const {return const_cast<smallVector*>(this)->small();}
        const big_t&   big()   const {return const_cast<smallVector*>(this)->big();}

        static constexpr size_t variantSize = std::max(sizeof(small_t), sizeof(big_t));

        alignas(void*) uint8_t _variant[variantSize];   // Storage for either a small_t or a big_t
        uint32_t               _size {0};               // Current item count
        bool                   _isBig {false};          // True if _storage contains a big_t
    };

}
