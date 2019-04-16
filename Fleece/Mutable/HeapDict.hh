//
// HeapDict.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "Dict.hh"
#include "ValueSlot.hh"
#include "SharedKeys.hh"
#include <deque>
#include <unordered_map>
#include <memory>

namespace fleece { namespace impl {
    class Encoder;
    class SharedKeys;
} }

namespace fleece { namespace impl { namespace internal {
    class HeapArray;

    class HeapDict : public HeapCollection {
    public:
        HeapDict(const Dict* =nullptr);

        static MutableDict* asMutableDict(HeapDict *a)      {return (MutableDict*)asValue(a);}
        MutableDict* asMutableDict() const                  {return (MutableDict*)asValue();}

        const Dict* source() const                          {return _source;}
        SharedKeys* sharedKeys() const                      {return _sharedKeys;}

        uint32_t count() const                              {return _count;}
        bool empty() const                                  {return _count == 0;}
        
        const Value* get(slice keyToFind) const noexcept;
        const Value* get(int keyToFind) const noexcept;
        const Value* get(Dict::key &keyToFind) const noexcept;
        const Value* get(const key_t &keyToFind) const noexcept;

        // Warning: Modifying a HeapDict invalidates all Dict::iterators on it!

        template <typename T>
        void set(slice key, T value)                        {setting(key).set(value);}

        ValueSlot& setting(slice key);

        void remove(slice key);
        void removeAll();

        /** Promotes an Array value to a MutableArray (in place) and returns it.
            Or if the value is already a MutableArray, just returns it. Else returns null. */
        MutableArray* getMutableArray(slice key)  {return (MutableArray*)asValue(getMutable(key, kArrayTag));}

        /** Promotes a Dict value to a HeapDict (in place) and returns it.
            Or if the value is already a HeapDict, just returns it. Else returns null. */
        MutableDict* getMutableDict(slice key)    {return (MutableDict*)asValue(getMutable(key, kDictTag));}

        void disconnectFromSource();
        void copyChildren(CopyFlags flags);

        void writeTo(Encoder&);


        using keyMap = std::map<key_t, ValueSlot>;


        class iterator {
        public:
            iterator(const HeapDict* NONNULL) noexcept;
            iterator(const MutableDict* NONNULL) noexcept;

            uint32_t count() const noexcept                 {return _count;}
            
            slice keyString() const noexcept                {return _key;}
            const Value* value() const noexcept             {return _value;}

            /** Returns false when the iterator reaches the end. */
            explicit operator bool() const noexcept          {return _value != nullptr;}

            /** Steps to the next item. (Throws if there are no more items.) */
            iterator& operator ++();

        private:
            void getSource();
            void getNew();
            void decodeKey(key_t);

            slice _key;
            const Value* _value;
            Dict::iterator _sourceIter;
            keyMap::const_iterator _newIter, _newEnd;
            bool _sourceActive, _newActive;
            key_t _sourceKey;
            uint32_t _count;
            SharedKeys* _sharedKeys;
        };


    protected:
        friend class fleece::impl::Array;
        friend class fleece::impl::MutableDict;

        ~HeapDict() =default;
        HeapArray* kvArray();

    private:
        key_t encodeKey(slice) const noexcept;
        void markChanged();
        key_t _allocateKey(key_t key);
        ValueSlot* _findValueFor(slice keyToFind) const noexcept;
        ValueSlot* _findValueFor(key_t keyToFind) const noexcept;
        ValueSlot& _makeValueFor(key_t key);
        HeapCollection* getMutable(slice key, tags ifType);
        bool tooManyAncestors() const;

        uint32_t _count {0};
        const Dict* _source {nullptr};
        Retained<SharedKeys> _sharedKeys;
        keyMap _map;
        std::deque<alloc_slice> _backingSlices;
        Retained<HeapArray> _iterable;
    };
    
} } }
