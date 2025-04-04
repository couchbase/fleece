//
// Dict.hh
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "Array.hh"
#include <memory>

namespace fleece { namespace impl {

    class DictIterator;
    class MutableDict;
    class SharedKeys;
    class key_t;

    /** A Value that's a dictionary/map */
    class Dict : public Value {
    public:
        /** The number of items in the dictionary. */
        uint32_t count() const noexcept FLPURE;

        bool empty() const noexcept FLPURE;

        /** Looks up the Value for a string key. */
        const Value* get(slice keyToFind) const noexcept FLPURE;

        /** Looks up the Value for an integer (shared) key. */
        const Value* get(int numericKeyToFind) const noexcept FLPURE;

        /** If this array is mutable, returns the equivalent MutableArray*, else returns nullptr. */
        MutableDict* asMutable() const noexcept FLPURE;

#ifdef __OBJC__
        /** Looks up the Value for a key given as an NSString object. */
        const Value* get(NSString* key NONNULL) const noexcept {
            nsstring_slice keyBytes(key);
            return get(keyBytes);
        }
#endif

        bool isEqualToDict(const Dict* NONNULL) const noexcept FLPURE;

        /** An empty Dict. */
        static const Dict* const kEmpty;

        using iterator = DictIterator;

        inline iterator begin() const noexcept;

        /** An abstracted key for dictionaries. It will cache the key's shared int value, and it
            will cache the index at which the key was last found, which speeds up succssive
            lookups.
            Warning: An instance of this should be used only on a single thread, and only with
            documents that share the same SharedKeys. */
        class key {
        public:
            /** Constructs a key from a string.
                Warning: the input string's memory MUST remain valid for as long as the key is in
                use! (The key stores a pointer to the string, but does not copy it.) */
            key(slice rawString)                         :_rawString(rawString) { }
            ~key();
            slice string() const noexcept                {return _rawString;}
            int compare(const key &k) const noexcept     {return _rawString.compare(k._rawString);}
            key(const key&) =delete;
            bool isShared() const noexcept               {return _hasNumericKey > 0;}
        private:
            void setSharedKeys(SharedKeys*);
            
            slice const _rawString;
            SharedKeys* _sharedKeys {nullptr};
            uint32_t _hint          {0xFFFFFFFF};
            int32_t _numericKey;
            int8_t _hasNumericKey   {0};

            template <bool WIDE> friend struct dictImpl;
        };

        /** Looks up the Value for a key, in a form that can cache the key's Fleece object.
            Using the Fleece object is significantly faster than a normal get. */
        const Value* get(key&) const noexcept;

        const Value* get(const key_t&) const noexcept;

        constexpr Dict()  :Value(internal::kDictTag, 0, 0) { }

    protected:
        internal::HeapDict* heapDict() const noexcept FLPURE;
        uint32_t rawCount() const noexcept FLPURE;
        const Dict* getParent() const noexcept FLPURE;

        // This is like `get` but returns the key _as stored in the Dict_, either slice or int.
        key_t encodeKey(slice keyString, SharedKeys *sharedKeys NONNULL) const noexcept;

        static bool isMagicParentKey(const Value *v);
        static constexpr int kMagicParentKey = -2048;

        template <bool WIDE> friend struct dictImpl;
        friend class DictIterator;
        friend class Value;
        friend class Encoder;
        friend class internal::HeapDict;
    };


    /** A stack-based dictionary iterator */
    class DictIterator {
    public:
        /** Constructs an iterator. It's OK for the Dict to be null. */
        DictIterator(const Dict*) noexcept;

        /** Constructs an iterator on a Dict using shared keys. It's OK for the Dict to be null. */
        DictIterator(const Dict*, const SharedKeys*) noexcept;

        /** Returns the number of _remaining_ items. */
        uint32_t count() const noexcept FLPURE                  {return _a._count;}

        slice keyString() const noexcept;
        const Value* key() const noexcept FLPURE                {return _key;}
        const Value* value() const noexcept FLPURE              {return _value;}

        /** Returns false when the iterator reaches the end. */
        explicit operator bool() const noexcept FLPURE          {return _key != nullptr;}

        /** Steps to the next item. (Throws if there are no more items.) */
        DictIterator& operator ++();

        /** Steps forward by one or more items. (Throws if stepping past the end.) */
        DictIterator& operator += (uint32_t);

        const SharedKeys* sharedKeys() const FLPURE            {return _sharedKeys;}
        key_t keyt() const noexcept;

#ifdef __OBJC__
        NSString* keyToNSString(NSMapTable *sharedStrings) const;
#endif

    private:
        DictIterator(const Dict* d, bool) noexcept;     // for Value::dump() only
        void readKV() noexcept;
        const Value* rawKey() noexcept             {return _a._first;}
        const Value* rawValue() noexcept           {return _a.second();}
        SharedKeys* findSharedKeys() const;

        Array::impl _a;
        const Value *_key, *_value;
        mutable const SharedKeys *_sharedKeys {nullptr};
        std::unique_ptr<DictIterator> _parent;
        int _keyCmp {-1};

        friend class Value;
        friend class ValueDumper;
        friend class Encoder;
    };


    inline Dict::iterator Dict::begin() const noexcept    {return iterator(this);}

} }
