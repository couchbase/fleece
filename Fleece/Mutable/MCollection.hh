//
//  MCollection.hh
//  Fleece
//
//  Created by Jens Alfke on 10/4/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "MValue.hh"

namespace fleece {

    /** Abstract superclass of MArray and MDict. Manages upward connections to slot & parent. */
    template <class Native>
    class MCollection {
    protected:
        using MValue = MValue<Native>;

        MCollection() =default;

        MCollection(const alloc_slice &data, SharedKeys *sk)
        :_data(data)
        ,_sharedKeys(sk)
        { }

        void init(MValue *slot, MCollection *parent) {
            assert(slot);
            _slot = slot;
            _parent = parent;
            if (parent) {
                _data = parent->_data;
                _sharedKeys = parent->_sharedKeys;
            }
        }

    public:
        void setSlot(MValue *newSlot, MValue *oldSlot) {
            if (_slot == oldSlot) {
                _slot = newSlot;
                if (!newSlot)
                    _parent = nullptr;
            }
        }

    protected:
        alloc_slice originalData() const            {return _data;}
        SharedKeys* sharedKeys() const              {return _sharedKeys;}
        
        bool isMutated() const {
            return !_slot || _slot->isMutated();
        }

        void mutate() {
            if (_slot)
                _slot->mutate();
            if (_parent)
                _parent->mutate();
        }

    private:
        MValue*         _slot {nullptr};            // Value representing this collection
        MCollection*    _parent {nullptr};          // Parent collection
        alloc_slice     _data;                      // Fleece data; ensures it doesn't go away
        SharedKeys*     _sharedKeys {nullptr};      // SharedKeys to use with Dicts
    };


    /** Top-level object. */
    template <class Native>
    class MRoot : private MCollection<Native> {
    public:
        using MCollection = MCollection<Native>;

        MRoot() =default;

        MRoot(alloc_slice fleeceData,
              SharedKeys *sk,
              const Value *value,
              bool mutableContainers =true)
        :MCollection(fleeceData, sk)
        ,_rootSlot(value)
        ,_mutableContainers(mutableContainers)
        {
            assert(value != nullptr);
            MCollection::init(&_rootSlot, nullptr);
        }

        MRoot(alloc_slice fleeceData,
              SharedKeys *sk =nullptr,
              bool mutableContainers =true)
        :MRoot(fleeceData, sk, Value::fromData(fleeceData), mutableContainers)
        { }

        explicit operator bool() const      {return !_rootSlot.isEmpty();}

        alloc_slice originalData() const    {return MCollection::originalData();}
        SharedKeys* sharedKeys() const      {return MCollection::sharedKeys();}

        Native asNative() const             {return _rootSlot.asNative(this, _mutableContainers);}
        bool isMutated() const              {return _rootSlot.isMutated();}
        void encodeTo(Encoder &enc) const   {_rootSlot.encodeTo(enc);}

    private:
        MValue<Native>  _rootSlot;              // Phantom slot containing me
        bool            _mutableContainers;     // Should arrays/dicts be created mutable?
    };

}
