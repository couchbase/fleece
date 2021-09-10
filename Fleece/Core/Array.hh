//
// Array.hh
//
// Copyright 2016-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once

#include "Value.hh"

namespace fleece { namespace impl {

    class Dict;
    class MutableArray;
    class ArrayIterator;

    /** A Value that's an array. */
    class Array : public Value {
        struct impl {
            const Value* _first;
            uint32_t _count;
            uint8_t _width;

            impl(const Value*) noexcept;
            const Value* second() const noexcept FLPURE      {return offsetby(_first, _width);}
            const Value* firstValue() const noexcept FLPURE;
            const Value* deref(const Value*) const noexcept FLPURE;
            const Value* operator[] (unsigned index) const noexcept FLPURE;
            size_t indexOf(const Value *v) const noexcept FLPURE;
            void offset(uint32_t n);
            bool isMutableArray() const noexcept FLPURE      {return _width > 4;}
        };

    public:

        /** The number of items in the array. */
        uint32_t count() const noexcept FLPURE;

        bool empty() const noexcept FLPURE;

        /** Accesses an array item. Returns nullptr for out of range index.
            If you're accessing a lot of items of the same array, it's faster to make an
            iterator and use its sequential or random-access accessors. */
        const Value* get(uint32_t index) const noexcept FLPURE;

        /** If this array is mutable, returns the equivalent MutableArray*, else returns nullptr. */
        MutableArray* asMutable() const FLPURE;

        /** An empty Array. */
        static const Array* const kEmpty;

        using iterator = ArrayIterator;

        inline iterator begin() const noexcept;

        constexpr Array()  :Value(internal::kArrayTag, 0, 0) { }

    protected:
        internal::HeapArray* heapArray() const;

    private:
        friend class Value;
        friend class ArrayIterator;
        friend class Dict;
        friend class DictIterator;
        template <bool WIDE> friend struct dictImpl;
        friend class internal::HeapArray;
    };


    /** A stack-based array iterator */
    class ArrayIterator : private Array::impl {
    public:
        /** Constructs an iterator. It's OK if the Array pointer is null. */
        ArrayIterator(const Array* a) noexcept;

        /** Returns the number of _remaining_ items. */
        uint32_t count() const noexcept FLPURE                  {return _count;}

        const Value* value() const noexcept FLPURE              {return _value;}
        explicit operator const Value* () const noexcept FLPURE {return _value;}
        const Value* operator-> () const noexcept FLPURE        {return _value;}

        /** Returns the current item and advances to the next. */
        const Value* read() noexcept                     {auto v = _value; ++(*this); return v;}

        /** Random access to items. Index is relative to the current item.
            This is very fast, faster than array::get(). */
        const Value* operator[] (unsigned i) const noexcept FLPURE    {return ((impl&)*this)[i];}

        /** Returns false when the iterator reaches the end. */
        explicit operator bool() const noexcept FLPURE          {return _count > 0;}

        /** Steps to the next item. (Throws if there are no more items.) */
        ArrayIterator& operator++();

        /** Steps forward by one or more items. (Throws if stepping past the end.) */
        ArrayIterator& operator += (uint32_t);

    private:
        const Value* rawValue() noexcept                 {return _first;}

        const Value *_value;

        friend class Value;
        friend class ValueDumper;
    };


    inline ArrayIterator Array::begin() const noexcept {return iterator(this);}

} }
