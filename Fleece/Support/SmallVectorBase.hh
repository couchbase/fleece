//
// SmallVectorBase.hh
//
// Copyright (C) 2020 Jens Alfke. All Rights Reserved.
//

//
// SmallVector.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "PlatformCompat.hh"
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string.h>
#include <utility>
#include "betterassert.hh"

namespace fleece {

    // Non-template base class of smallVector<T,N>. As much code as possible is put here so that it
    // can be shared between all smallVector template instantiations.
    class smallVectorBase {
    public:
        size_t size() const FLPURE                      {return _size;}
        bool empty() const FLPURE                       {return _size == 0;}

        /// The maximum size a smallVector can grow to.
        static constexpr size_t max_size = (1ul<<31) - 1;

    protected:
        smallVectorBase() noexcept                      :_isBig(false), _size(0) { }


        // Throws if size/capacity is too big, and returns cast to uint32_t
        static uint32_t rangeCheck(size_t n) {
            if (_usuallyFalse(n > max_size))
                throw std::domain_error("smallVector size/capacity too large");
            return uint32_t(n);
        }


        // Pointer to first item
        void* _begin() FLPURE {
            return _isBig ? _big().begin() : _small().begin();
        }


        // Moves `sv` to myself.
        void _moveFrom(smallVectorBase &&sv, size_t instanceSize) {
            ::memcpy(this, &sv, instanceSize);
            std::move(sv).release();
        }


        // Clears a moved-from instance so destructor will be a no-op
        void release() && {
            _size = 0;
            if (_isBig)
                _big().release();
        }


        uint32_t _bigCapacity() const FLPURE {
            assert_precondition(_isBig);
            return const_cast<smallVectorBase*>(this)->_big().capacity();
        }


        // Adjusts capacity, possibly switching between big/small storage.
        void _setCapacity(size_t cap, size_t itemSize, bool embiggen) {
            precondition(cap >= _size);
            if (embiggen) {
                uint32_t newCap = rangeCheck(cap);
                if (_isBig) {
                    _big().setCapacity(newCap, _size, itemSize);
                } else {
                    big_t newBig(newCap, _small().begin(), _size, itemSize);
                    new (&_variant) big_t(std::move(newBig));
                    _isBig = true;
                }
            } else {
                if (_isBig) {
                    big_t oldBig = std::move(_big());
                    new (&_variant) small_t(oldBig.begin(), _size, itemSize);
                    _isBig = false;
                }
            }
        }


        // Increments size & returns pointer to (uninitialized) new final item.
        void* _growByOne(uint32_t capacity, size_t itemSize) {
            auto oldSize = _size;
            if (_usuallyTrue(oldSize < capacity))
                ++_size;
            else
                _growTo(oldSize + 1, capacity, itemSize);
            return offsetby(_begin(), oldSize * itemSize);
        }


        void _growTo(uint32_t newSize, uint32_t capacity, size_t itemSize) {
            assert_precondition(newSize >= _size);
            if (_usuallyFalse(newSize > capacity)) {
                // Compute new capacity: grow by 50% until big enough.
                if (newSize < (max_size / 3) * 2) {
                    do {
                        capacity += capacity / 2;
                    } while (capacity < newSize);
                } else {
                    capacity = max_size;
                }
                _setCapacity(capacity, itemSize, true);
            }
            _size = newSize;
        }


        // Inserts space for a new item at `where` and returns a pointer to it.
        void* _insertOne(void *where, uint32_t capacity, size_t itemSize) {
            auto begin = (uint8_t*)_begin();
            if (_usuallyFalse(_size >= capacity)) {
                // [calling grow() will reallocate storage, so save & restore `where`]
                size_t offset = (uint8_t*)where - begin;
                _growByOne(capacity, itemSize);
                begin = (uint8_t*)_begin();
                where = begin + offset;
            } else {
                ++_size;
            }
            _moveItems((uint8_t*)where + itemSize,
                       where,
                       begin + (_size - 1) * itemSize);
            return where;
        }


        // Moves the items in the range [srcStart..srcEnd) to dst
        static void _moveItems(void *dst, void *srcStart, void *srcEnd) {
            assert(srcStart <= srcEnd);
            auto n = (uint8_t*)srcEnd - (uint8_t*)srcStart;
            if (n > 0)
                ::memmove(dst, srcStart, n);
        }


        //---- STORAGE:  This class uses a custom smart union similar to C++17's `variant`.


        // Small variant, with inline buffer
        class small_t {
        public:
            small_t(void *items, uint32_t size, size_t itemSize) noexcept {
                ::memmove(_buffer, items, size * itemSize);
            }

            void* begin() noexcept FLPURE               {return _buffer;}

        protected:
            uint8_t _buffer[1];
        };


        // Large variant, with heap-allocated buffer
        class big_t {
        public:
            big_t(uint32_t capacity, size_t itemSize)
            :_buffer(std::make_unique<uint8_t[]>(capacity * itemSize))
            ,_capacity(capacity)
            { }

            big_t(uint32_t capacity, void *items, uint32_t size, size_t itemSize)
            :big_t(capacity, itemSize)
            {
                ::memmove(_buffer.get(), items, size * itemSize);
            }

            big_t(big_t &&other) noexcept = default;

            void release() noexcept                     {_buffer.release();}

            void* begin() noexcept FLPURE               {return _buffer.get();}

            uint32_t capacity() const noexcept FLPURE   {return _capacity;}

            void setCapacity(uint32_t newCapacity, uint32_t size, size_t itemSize) {
                assert_precondition(newCapacity >= size && _capacity >= size);
                //OPT: Would be more optimal to use `realloc`, as it can resize in place
                auto newBytes = std::make_unique<uint8_t[]>(newCapacity * itemSize);
                ::memmove(newBytes.get(), _buffer.get(), size * itemSize);
                _buffer = std::move(newBytes);
                _capacity = newCapacity;
            }

        private:
            std::unique_ptr<uint8_t[]>  _buffer;
            uint32_t                    _capacity = 0;
        };


        small_t& _small()       {assert(!_isBig); return *(small_t*)&_variant;}
        big_t&   _big()         {assert(_isBig); return *(big_t*)&_variant;}


        uint32_t               _size;           // Current item count
        bool                   _isBig;          // True if _storage contains a big_t
        alignas(void*) uint8_t _variant[sizeof(big_t)];  // Start of storage for either a small_t or a big_t
    };

}
