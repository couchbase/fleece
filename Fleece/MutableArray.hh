//
//  MutableArray.hh
//  Fleece
//
//  Created by Jens Alfke on 9/19/17.
//Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "Array.hh"

namespace fleece {
    class MutableArray;
    class MutableDict;


    namespace internal {
        class MutableValue : public Value {
        public:
            MutableValue()
            :Value(internal::kSpecialTag, internal::kSpecialValueNull) { }

            static const Value* derefPointer(const MutableValue *v) {
                return (Value*)( _dec64(*(size_t*)v) << 1 );
            }

            const Value* deref() const {
                return isPointer() ? derefPointer(this) : this;
            }

            void set(internal::tags tag, int tiny) {
                _byte[0] = (uint8_t)((tag<<4) | tiny);
            }

            void set(internal::tags tag, int tiny, int byte1) {
                set(tag, tiny);
                _byte[1] = (uint8_t)byte1;
            }

            void set(Null);
            void set(bool);
            void set(int i)         {set((int64_t)i);}
            void set(int64_t);
            void set(uint64_t);
            void set(slice s)       {set(internal::kStringTag, s);}
            void set(const Value*);

            void copy(const Value*);

            MutableArray* makeArrayMutable();
            MutableDict* makeDictMutable();

        private:
            void set(uint64_t i, bool isSmall, bool isUnsigned);
            void set(internal::tags, slice s);

            uint8_t _moreBytes[6];  // For a total of 10 bytes, enough to hold all value types
        };
    }


    class MutableArray : public Array {
    public:
        MutableArray()
        :Array(internal::kSpecialTag, internal::kSpecialValueMutableArray)
        { }

        MutableArray(uint32_t count)
        :Array(internal::kSpecialTag, internal::kSpecialValueMutableArray)
        ,_items(count)
        { }

        MutableArray(const Array*);

        uint32_t count() const                      {return (uint32_t)_items.size();}

        template <typename T>
        void set(uint32_t index, T t)               {_items[index].set(t);}

        MutableArray* makeArrayMutable(uint32_t i)  {return _items[i].makeArrayMutable();}
        MutableDict* makeDictMutable(uint32_t i)    {return _items[i].makeDictMutable();}

        // Warning: Changing the size of a MutableArray invalidates all Array::iterators on it!

        template <typename T>
        void append(T t) {
            _items.emplace_back();
            _items.back().set(t);
        }
        
        void resize(uint32_t newSize);
        void insert(uint32_t where, uint32_t n);
        void remove(uint32_t where, uint32_t n);

    protected:
        const internal::MutableValue* first() const           {return &_items[0];}

    private:
        std::vector<internal::MutableValue> _items;

        friend class Array::impl;
    };
}
