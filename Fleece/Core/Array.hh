//
// Array.hh
//
// Copyright (c) 2016 Couchbase, Inc All rights reserved.
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

#include "Value.hh"

namespace fleece { namespace impl {

    class Dict;
    class MutableArray;

    /** A Value that's an array. */
    class Array : public Value {
        struct impl {
            const Value* _first;
            uint32_t _count;
            uint8_t _width;

            impl(const Value*) noexcept;
            const Value* second() const noexcept      {return offsetby(_first, _width);}
            const Value* firstValue() const noexcept;
            const Value* deref(const Value*) const noexcept;
            const Value* operator[] (unsigned index) const noexcept;
            size_t indexOf(const Value *v) const noexcept;
            void offset(uint32_t n);
            bool isMutableArray() const                 {return _width > 4;}
        };

    public:

        /** The number of items in the array. */
        uint32_t count() const noexcept;

        bool empty() const noexcept                         {return countIsZero();}

        /** Accesses an array item. Returns nullptr for out of range index.
            If you're accessing a lot of items of the same array, it's faster to make an
            iterator and use its sequential or random-access accessors. */
        const Value* get(uint32_t index) const noexcept;

        /** If this array is mutable, returns the equivalent MutableArray*, else returns nullptr. */
        MutableArray* asMutable() const;

        /** An empty Array. */
        static const Array* const kEmpty;

        /** A stack-based array iterator */
        class iterator : private impl {
        public:
            /** Constructs an iterator. It's OK if the Array pointer is null. */
            iterator(const Array* a) noexcept;

            /** Returns the number of _remaining_ items. */
            uint32_t count() const noexcept                  {return _count;}

            const Value* value() const noexcept              {return _value;}
            explicit operator const Value* () const noexcept {return _value;}
            const Value* operator-> () const noexcept        {return _value;}

            /** Returns the current item and advances to the next. */
            const Value* read() noexcept                     {auto v = _value; ++(*this); return v;}

            /** Random access to items. Index is relative to the current item.
                This is very fast, faster than array::get(). */
            const Value* operator[] (unsigned i) const noexcept    {return ((impl&)*this)[i];}

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
        internal::HeapArray* heapArray() const;

    private:
        friend class Value;
        friend class Dict;
        template <bool WIDE> friend struct dictImpl;
    };

} }
