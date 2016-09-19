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
        struct impl {
            const Value* _first;
            uint32_t _count;
            bool _wide;

            impl(const Value*) noexcept;
            const Value* second() const noexcept      {return _first->next(_wide);}
            bool next();
            const Value* firstValue() const noexcept  {return _count ? Value::deref(_first, _wide) : NULL;}
            const Value* operator[] (unsigned index) const noexcept;
            size_t indexOf(const Value *v) const noexcept;
        };

    public:

        /** The number of items in the array. */
        uint32_t count() const noexcept;

        /** Accesses an array item. Returns NULL for out of range index.
            If you're accessing a lot of items of the same array, it's faster to make an
            iterator and use its sequential or random-access accessors. */
        const Value* get(uint32_t index) const noexcept;

        /** A stack-based array iterator */
        class iterator {
        public:
            iterator(const Array* a) noexcept;

            /** Returns the number of _remaining_ items. */
            uint32_t count() const noexcept              {return _a._count;}

            const Value* value() const noexcept          {return _value;}
            explicit operator const Value* const () noexcept {return _value;}
            const Value* operator-> () noexcept          {return _value;}

            /** Returns the current item and advances to the next. */
            const Value* read() noexcept                 {auto v = _value; ++(*this); return v;}

            /** Random access to items. Index is relative to the current item.
                This is very fast, faster than array::get(). */
            const Value* operator[] (unsigned i) noexcept {return _a[i];}

            /** Returns false when the iterator reaches the end. */
            explicit operator bool() const noexcept      {return _a._count > 0;}

            /** Steps to the next item. (Throws if there are no more items.) */
            iterator& operator++();

            /** Steps forward by one or more items. (Throws if stepping past the end.) */
            iterator& operator += (uint32_t);

        private:
            const Value* rawValue() noexcept             {return _a._first;}

            impl _a;
            const Value *_value;
            
            friend class Value;
        };

        iterator begin() const noexcept                  {return iterator(this);}

        friend class Value;
        friend class Dict;
        template <bool WIDE> friend struct dictImpl;
    };


    /** A Value that's a dictionary/map */
    class Dict : public Value {
    public:
        /** The number of items in the dictionary. */
        uint32_t count() const noexcept;

        /** Looks up the Value for a key, assuming the keys are sorted (as they are by default.) */
        const Value* get(slice keyToFind) const noexcept;

        /** Looks up the Value for a key, without assuming the keys are sorted.
            This is slower than get(), but works even if the Fleece data was generated without
            sorted keys. */
        const Value* get_unsorted(slice key) const noexcept;

#ifdef __OBJC__
        /** Looks up the Value for a key given as an NSString object. */
        const Value* get(NSString* key) const noexcept {
            nsstring_slice keyBytes(key);
            return get(keyBytes);
        }
#endif

        /** A stack-based dictionary iterator */
        class iterator {
        public:
            iterator(const Dict*) noexcept;

            /** Returns the number of _remaining_ items. */
            uint32_t count() const noexcept                  {return _a._count;}

            const Value* key() const noexcept                {return _key;}
            const Value* value() const noexcept              {return _value;}

            /** Returns false when the iterator reaches the end. */
            explicit operator bool() const noexcept          {return _a._count > 0;}

            /** Steps to the next item. (Throws if there are no more items.) */
            iterator& operator ++();

            /** Steps forward by one or more items. (Throws if stepping past the end.) */
            iterator& operator += (uint32_t);

        private:
            void readKV() noexcept;
            const Value* rawKey() noexcept             {return _a._first;}
            const Value* rawValue() noexcept           {return _a.second();}

            Array::impl _a;
            const Value *_key, *_value;
            friend class Value;
        };
        
        iterator begin() const noexcept                      {return iterator(this);}

        /** An abstracted key for dictionaries. It will cache the key as an encoded Value, and it
            will cache the index at which the key was last found, which speeds up succssive
            lookups.
            Warning: An instance of this should be used only on a single thread.
            Warning: If you set the `cache` flag to true, the key will cache the Value
            representation of the string, so it should only be used with dictionaries that are
            stored in the same encoded data. */
        class key {
        public:
            key(slice rawString, bool cachePointer = false) noexcept
            :_rawString(rawString), _cachePointer(cachePointer) { }

            const Value* asValue() const noexcept            {return _keyValue;}
            int compare(const key &k) const noexcept         {return _rawString.compare(k._rawString);}
        private:
            slice const _rawString;
            const Value* _keyValue  {nullptr};
            uint32_t _hint          {0xFFFFFFFF};
            bool _cachePointer;

            template <bool WIDE> friend struct dictImpl;
        };

        /** Looks up the Value for a key, in a form that can cache the key's Fleece object.
            Using the Fleece object is significantly faster than a normal get. */
        const Value* get(key&) const noexcept;

        /** Looks up multiple keys at once; this can be a lot faster than multiple gets.
            @param keys  Array of key objects. MUST be sorted lexicographically in increasing order.
            @param values  The corresponding values (or NULLs) will be written here.
            @param count  The number of keys and values.
            @return  The number of keys that were found. */
        size_t get(key keys[], const Value* values[], size_t count) const noexcept;

        /** Sorts an array of keys, a prerequisite of the multi-key get() method. */
        static void sortKeys(key keys[], size_t count) noexcept;

    private:
        friend class Value;
    };

}
