//
// SmallVector.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "PlatformCompat.hh"
#include <array>
#include <stdexcept>
#include "betterassert.hh"

namespace fleece {

    template <class T, size_t N>
    class smallVector {
    public:
        smallVector()                               { }
        ~smallVector()                              {clear(); free(_big);}

        smallVector(size_t size)                    :smallVector() {resize(size);}

        smallVector(smallVector &&sv)
        :_size(sv._size)
        ,_capacity(sv._capacity)
        ,_big(sv._big)
        {
            sv._size = 0;
            if (_big)
                sv._big = nullptr;
            else
                memcpy(_small, sv._small, _size * sizeof(T));
            assert_postcondition(_size <= _capacity);
        }

        smallVector& operator=(smallVector &&sv) noexcept {
            _size = sv._size;
            sv._size = 0;
            _capacity = sv._capacity;
            free(_big);
            _big = sv._big;
            if (_big)
                sv._big = nullptr;
            else
                memcpy(_small, sv._small, _size * sizeof(T));
            return *this;
        }

        template <size_t M>
        bool operator== (const smallVector<T,M> &v) const {
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

        size_t size() const FLPURE                         {return _size;}
        size_t capacity() const FLPURE                     {return _capacity;}

        bool empty() const FLPURE                          {return _size == 0;}
        void clear()                                {shrinkTo(0);}
        void reserve(size_t cap)                    {if (cap>_capacity) setCapacity(cap);}

        const T& get(size_t i) const FLPURE {
            assert_precondition(i < _size);
            return _get(i);
        }

        T& get(size_t i) FLPURE {
            assert_precondition(i < _size);
            return _get(i);
        }

        const T& operator[] (size_t i) const FLPURE   {return get(i);}
        T& operator[] (size_t i) FLPURE               {return get(i);}
        const T& back() const FLPURE                  {return get(_size - 1);}
        T& back() FLPURE                              {return get(_size - 1);}

        using iterator = T*;
        using const_iterator = const T*;

        iterator begin() FLPURE                            {return &_get(0);}
        iterator end() FLPURE                              {return &_get(_size);}
        const_iterator begin() const FLPURE                {return &_get(0);}
        const_iterator end() const FLPURE                  {return &_get(_size);}

        T& push_back(const T& t)                    {return * new(_grow()) T(t);}
        T& push_back(T&& t)                         {return * new(_grow()) T(t);}

        void* push_back()                           {return _grow();}

        void pop_back()                             {get(_size - 1).~T(); --_size;}

        template <class... Args>
        T& emplace_back(Args&&... args) {
            return * new(_grow()) T(std::forward<Args>(args)...);
        }

        void insert(iterator where, T item) {
            assert_precondition(begin() <= where && where <= end());
            if (_usuallyFalse(_size >= _capacity)) {
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
            memmove(first, first + 1, (end() - first - 1) * sizeof(T));
            --_size;
        }

        void erase(iterator first, iterator last) {
            assert_precondition(begin() <= first && first < last && last <= end());
            for (auto i = first; i < last; ++i)
                i->T::~T();                 // destruct removed items
            memmove(first, last, (end() - last) * sizeof(T));
            _size -= last - first;
        }

        void resize(size_t sz) {
            if (sz > _size) {
                if (_usuallyFalse(sz > _capacity)) {
                    auto cap = sz;
                    if (cap > N)
                        cap = std::max((size_t)_capacity + _capacity/2, cap);
                    setCapacity(cap);
                }
                auto i = _size;
                _size = (uint32_t)sz;
                for (; i < sz; ++i)
                    (void) new (&get(i)) T();       // construct new item
            } else {
                shrinkTo(sz);
            }
        }

        void setCapacity(size_t cap) {
            if (cap == _capacity)
                return;
            if (_usuallyFalse(cap < _size))
                throw std::logic_error("capacity smaller than size");
            if (_usuallyFalse(cap > UINT32_MAX))
                throw std::domain_error("capacity too large");
            if (_usuallyFalse(cap <= N)) {
                if (_big) {
                    // Switch to _small:
                    memcpy(_small, _big, _size * sizeof(T));
                    free(_big);
                    _big = nullptr;
                }
            } else {
                auto newBig = (T*)realloc(_big, cap * sizeof(T));
                if (!newBig)
                    throw std::bad_alloc();
                if (!_big) {
                    // Switch to _big:
                    memcpy(newBig, _small, _size * sizeof(T));
                }
                _big = newBig;
            }
            _capacity = (uint32_t)cap;
        }

    private:
        smallVector(const smallVector&) =delete;
        smallVector& operator=(const smallVector&) =delete;

        T& _get(size_t i) FLPURE {
            T *base = _usuallyFalse(_big != nullptr) ? _big : (T*)&_small;
            return base[i];
        }

        const T& _get(size_t i) const FLPURE {
            return const_cast<smallVector*>(this)->_get(i);
        }

        // Grow size by one and return new (unconstructed!) item
        T* _grow() {
            if (_usuallyFalse(_size >= _capacity)) {
                setCapacity(std::max((size_t)_capacity + _capacity/2,
                                     (size_t)_size + 1));
            }
            return &_get(_size++);
        }

        void shrinkTo(size_t sz) {
            if (sz < _size) {
                for (auto i = sz; i < _size; ++i)
                    get(i).T::~T();                 // destruct removed item
                _size = (uint32_t)sz;
            }
        }

        struct baseType { T t[N]; };

        uint32_t _size {0};
        uint32_t _capacity {N};
        char _small[sizeof(baseType)];   // really T[N], but avoids calling constructor
        T* _big {nullptr};
    };

}
