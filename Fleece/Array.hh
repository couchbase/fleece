//
//  Array.hh
//  Fleece
//
//  Created by Jens Alfke on 5/12/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//

#pragma once
#include "Value.hh"

namespace fleece {

    class Dict;

    /** A Value that's an array. */
    class Array : public Value {
    public:

        /** The number of items in the array. */
        uint32_t count() const noexcept;

        bool empty() const noexcept                         {return countIsZero();}

        /** Accesses an array item. Returns nullptr for out of range index.
            If you're accessing a lot of items of the same array, it's faster to make an
            iterator and use its sequential or random-access accessors. */
        const Value* get(uint32_t index) const noexcept;

        /** An empty Array. */
        static const Array* const kEmpty;

    private:
        struct impl {
            impl(const Value*) noexcept;

            const Value* _first;
            uint32_t _count;
            uint8_t _wide;

            const Value* second() const noexcept            {return _first->next(_wide);}
            const Value* firstValue() const noexcept;
            const Value* deref(const Value*) const noexcept;
            const Value* operator[] (unsigned index) const noexcept;
            size_t indexOf(const Value *v) const noexcept;
            void offset(uint32_t n);
        };

    public:
        /** A stack-based array iterator */
        class iterator : private impl {
        public:
            iterator(const Array* a) noexcept;

            /** Returns the number of _remaining_ items. */
            uint32_t count() const noexcept                  {return _count;}

            const Value* value() const noexcept              {return _value;}
            explicit operator const Value* const () noexcept {return _value;}
            const Value* operator-> () const noexcept        {return _value;}

            /** Returns the current item and advances to the next. */
            const Value* read() noexcept                     {auto v = _value; ++(*this); return v;}

            /** Random access to items. Index is relative to the current item.
                This is very fast, faster than array::get(). */
            const Value* operator[] (unsigned i) noexcept    {return impl::operator[](i);}

            /** Returns false when the iterator reaches the end. */
            explicit operator bool() const noexcept          {return _count > 0;}

            /** Steps to the next item. (Throws if there are no more items.) */
            iterator& operator++();

            /** Steps forward by one or more items. (Throws if stepping past the end.) */
            iterator& operator += (uint32_t);

        private:
            const Value* rawValue() noexcept                 {return _first;}

            const Value *_value;
            
            friend class Value;
        };

        iterator begin() const noexcept                      {return iterator(this);}

        constexpr Array()  :Value(internal::kArrayTag, 0, 0) { }

    protected:
        constexpr Array(internal::tags tag, int tiny, int byte1 = 0)  :Value(tag, tiny, byte1) { }
    private:
        friend class Value;
        friend class Dict;
        friend class MutableArray;
        friend class MutableDict;
        template <bool WIDE> friend struct dictImpl;
    };

}
