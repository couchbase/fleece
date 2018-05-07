//
// Dict.hh
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
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
#include "Array.hh"

namespace fleece {

    class MutableDict;
    class SharedKeys;

    /** A Value that's a dictionary/map */
    class Dict : public Value {
    public:
        /** The number of items in the dictionary. */
        uint32_t count() const noexcept;

        bool empty() const noexcept                         {return countIsZero();}

        /** Looks up the Value for a string key, assuming the keys are sorted
            (as they are by default.) */
        const Value* get(slice keyToFind) const noexcept;

        const Value* get(slice keyToFind, SharedKeys*) const noexcept;

        /** Looks up the Value for an integer key, assuming the keys are sorted
            (as they are by default.) */
        const Value* get(int numericKeyToFind) const noexcept;

        /** Looks up the Value for a string key, without assuming the keys are sorted.
            This is slower than get(), but works even if the Fleece data was generated without
            sorted keys. */
        const Value* get_unsorted(slice key) const noexcept;

        /** If this array is mutable, returns the equivalent MutableArray*, else returns nullptr. */
        MutableDict* asMutable() const;

#ifdef __OBJC__
        /** Looks up the Value for a key given as an NSString object. */
        const Value* get(NSString* key NONNULL) const noexcept {
            nsstring_slice keyBytes(key);
            return get(keyBytes);
        }
#endif

        /** An empty Dict. */
        static const Dict* const kEmpty;

        /** A stack-based dictionary iterator */
        class iterator {
        public:
            /** Constructs an iterator. It's OK for the Dict to be null. */
            iterator(const Dict*) noexcept;

            /** Constructs an iterator on a Dict using shared keys. It's OK for the Dict to be null. */
            iterator(const Dict*, const SharedKeys*) noexcept;

            /** Returns the number of _remaining_ items. */
            uint32_t count() const noexcept                  {return _a._count;}

            slice keyString() const noexcept;
            const Value* key() const noexcept                {return _key;}
            const Value* value() const noexcept              {return _value;}

            /** Returns false when the iterator reaches the end. */
            explicit operator bool() const noexcept          {return _a._count > 0;}

            /** Steps to the next item. (Throws if there are no more items.) */
            iterator& operator ++();

            /** Steps forward by one or more items. (Throws if stepping past the end.) */
            iterator& operator += (uint32_t);

#ifdef __OBJC__
            NSString* keyToNSString(NSMapTable *sharedStrings) const;
#endif

        private:
            void readKV() noexcept;
            const Value* rawKey() noexcept             {return _a._first;}
            const Value* rawValue() noexcept           {return _a.second();}

            Array::impl _a;
            const Value *_key, *_value;
            const SharedKeys *_sharedKeys {nullptr};

            friend class Value;
        };

        iterator begin() const noexcept                      {return iterator(this);}
        iterator begin(const SharedKeys *sk) const noexcept  {return iterator(this, sk);}

        /** An abstracted key for dictionaries. It will cache the key as an encoded Value, and it
            will cache the index at which the key was last found, which speeds up succssive
            lookups.
            Warning: An instance of this should be used only on a single thread.
            Warning: If you set the `cache` flag to true, the key will cache the Value
            representation of the string, so it should only be used with dictionaries that are
            stored in the same encoded data. */
        class key {
        public:
            /** Constructs a key from a string.
                Warning: the input string's memory MUST remain valid for as long as the key is in
                use! (The key stores a pointer to the string, but does not copy it.) */
            key(slice rawString);

            /** Constructs a key from a string. If the data was encoded using a SharedKeys mapping,
                you need to use this constructor so the proper numeric encoding can be found & used.
                Warning: the input string's memory MUST remain valid for as long as the key is in
                use! (The key may store a pointer to the string, but does not copy it.) */
            key(slice rawString, SharedKeys*, bool cachePointer =false);

            slice string() const noexcept                {return _rawString;}
            const Value* asValue() const noexcept        {return _keyValue;}
            int compare(const key &k) const noexcept     {return _rawString.compare(k._rawString);}
        private:
            slice const _rawString;
            const Value* _keyValue  {nullptr};
            SharedKeys* _sharedKeys {nullptr};
            uint32_t _hint          {0xFFFFFFFF};
            int32_t _numericKey;
            bool _cachePointer;
            bool _hasNumericKey     {false};

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

        constexpr Dict()  :Value(internal::kDictTag, 0, 0) { }

    protected:
        internal::HeapDict* heapDict() const;

    private:
        friend class Value;
    };

}
