//
// HeapDict.hh
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "Dict.hh"
#include "ValueSlot.hh"
#include "SharedKeys.hh"
#include <deque>
#include <map>

namespace fleece { namespace impl {
    class Encoder;
    class SharedKeys;
} }

namespace fleece { namespace impl { namespace internal {
    class HeapArray;

    class HeapDict : public HeapCollection {
    public:
        HeapDict(const Dict* =nullptr, CopyFlags flags =kDefaultCopy);

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
            iterator(const HeapDict* FL_NONNULL) noexcept;
            iterator(const MutableDict* FL_NONNULL) noexcept;

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
        }; // end of `iterator`


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

        uint32_t _count {0};                        // Dict's actual count
        RetainedConst<Dict> _source;                // Original Dict I shadow, if any
        Retained<SharedKeys> _sharedKeys;           // Namespace of integer keys
        keyMap _map;                                // Actual storage of key-value pairs
        std::deque<alloc_slice> _backingSlices;     // Backing storage of key slices
        Retained<HeapArray> _iterable;              // All key-value pairs in sequence, for iterator
    };
    
} } }
