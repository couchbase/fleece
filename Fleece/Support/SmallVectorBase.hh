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
    class alignas(void*) smallVectorBase {
    public:
        size_t capacity() const FLPURE                  {return _capacity;}
        size_t size() const FLPURE                      {return _size;}
        bool empty() const FLPURE                       {return _size == 0;}

        /// The maximum size a smallVector can grow to.
        static constexpr size_t max_size = (1ul<<31) - 1;

    protected:
        smallVectorBase() noexcept                      { }
        smallVectorBase(uint32_t cap) noexcept          : _size(0), _capacity(cap), _isBig(false) { }

        ~smallVectorBase() {
            if (_isBig)
                ::free(_dataPointer);
        }


        // Throws if size/capacity is too big, and returns cast to uint32_t
        static uint32_t rangeCheck(size_t n) {
            if (_usuallyFalse(n > max_size))
                throw std::domain_error("smallVector size/capacity too large");
            return uint32_t(n);
        }


        // Pointer to first item
        void* _begin() FLPURE                       {return _isBig ? _dataPointer : _inlineData;}
        const void* _begin() const FLPURE           {return _isBig ? _dataPointer : _inlineData;}


        // Moves `sv` to myself.
        void _moveFrom(smallVectorBase &&sv, size_t instanceSize) noexcept {
            ::memcpy(this, &sv, instanceSize);
            std::move(sv).release();
        }


        // Clears a moved-from instance, so destructor will be a no-op
        void release() && noexcept {
            _size = 0;
            _dataPointer = nullptr;
        }


        // Increases capacity, switching to external storage.
        void _embiggen(size_t cap, size_t itemSize) {
            precondition(cap >= _size);
            uint32_t newCap = rangeCheck(cap);
            void *pointer = _isBig ? _dataPointer : nullptr;
            pointer = ::realloc(pointer, newCap * itemSize);
            if (_usuallyFalse(!pointer))
                throw std::bad_alloc();
            if (!_isBig) {
                if (_size > 0)
                    ::memcpy(pointer, _inlineData, _size * itemSize);
                _isBig = true;
            }
            _dataPointer = pointer;
            _capacity = newCap;
        }

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
// Seems like GCC gets confused about the union type, potentially...

        // Reduces capacity, switching from external to inline storage.
        void _emsmallen(uint32_t newCap, size_t itemSize) {
            assert_precondition(_isBig);
            void *pointer = _dataPointer;
            if (_size > 0)
                ::memcpy(_inlineData, pointer, _size * itemSize);
            ::free(pointer);
            _isBig = false;
            _capacity = newCap;
        }

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

        // Increases size & returns pointer to (uninitialized) first new item.
        void* _growTo(uint32_t newSize, size_t itemSize) {
            auto oldSize = _size;
            assert_precondition(newSize >= oldSize);
            if (_usuallyFalse(newSize > _capacity)) {
                // Compute new capacity: grow by 50% until big enough.
                rangeCheck(newSize);
                uint32_t newCapacity;
                if (oldSize == 0) {
                    newCapacity = newSize;
                } else if (newSize >= (max_size / 3) * 2) {
                    newCapacity = max_size;         // (adding 50% would overflow max_size)
                } else {
                    newCapacity = _capacity;
                    do {
                        newCapacity += newCapacity / 2;
                    } while (newCapacity < newSize);
                }
                _embiggen(newCapacity, itemSize);
            }
            _size = newSize;
            return (uint8_t*)_begin() + oldSize * itemSize;
        }


        // Inserts space for `nItems` new items at `where` and returns a pointer to it.
        void* _insert(void *where, uint32_t nItems, size_t itemSize) {
            auto begin = (uint8_t*)_begin();
            if (_size + nItems <= _capacity) {
                _size += nItems;
            } else {
                // [calling _growTo() will reallocate storage, so save & restore `where`]
                size_t offset = (uint8_t*)where - begin;
                _growTo(_size + nItems, itemSize);
                begin = (uint8_t*)_begin();
                where = begin + offset;
            }
            _moveItems((uint8_t*)where + nItems * itemSize,
                       where,
                       begin + (_size - nItems) * itemSize);
            return where;
        }


        // Moves the items in the range [srcStart..srcEnd) to dst
        static void _moveItems(void *dst, void *srcStart, void *srcEnd) noexcept {
            assert(srcStart <= srcEnd);
            auto n = (uint8_t*)srcEnd - (uint8_t*)srcStart;
            if (n > 0)
                ::memmove(dst, srcStart, n);
        }

    protected:
        static constexpr size_t kBaseInlineCap = sizeof(void*);

        uint32_t    _size;                      // Current item count
        uint32_t    _capacity :31;              // Current max size before I have to realloc
        bool        _isBig    : 1;              // True if storage is heap-allocated
        union {
            void*   _dataPointer;               // Malloced pointer to data (when _isBig)
            uint8_t _inlineData[kBaseInlineCap];// Data starts here (when !_isBig)
        };
    };

}
