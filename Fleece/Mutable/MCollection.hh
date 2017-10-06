//
//  MCollection.hh
//  Fleece
//
//  Created by Jens Alfke on 10/4/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "MValue.hh"
#include <atomic>
#include <iostream>

namespace fleece {

    struct Context {
        Context(const alloc_slice &data, SharedKeys *sk, bool mutableContainers)
        :_data(data)
        ,_sharedKeys(sk)
        ,_mutableContainers(mutableContainers)
        {
            //std::cerr << "INIT Context " << this << "\n";
#if DEBUG
            ++gInstanceCount;
#endif
        }

#if DEBUG
        static std::atomic_int gInstanceCount;

        virtual ~Context() {
            assert(this != gNullContext);
            --gInstanceCount;
            //std::cerr << "DTOR Context " << this << "\n";
        }
#endif

        Context()
        :_refCount(0x7FFFFFFF)
        { }

        static Context* gNullContext;

        alloc_slice      _data;                     // Fleece data; ensures it doesn't go away
        SharedKeys*      _sharedKeys {nullptr};     // SharedKeys to use with Dicts
        std::atomic_uint _refCount {0};             // Reference count
        bool             _mutableContainers {true}; // Should arrays/dicts be created mutable?
    };

    static inline Context* retain(Context *ctx) {
        ++ctx->_refCount;
        return ctx;
    }

    static inline void release(Context *ctx) {
        if (--ctx->_refCount == 0)
            delete ctx;
    }


    /** Abstract superclass of MArray and MDict. Manages upward connections to slot & parent. */
    template <class Native>
    class MCollection {
    protected:
        using MValue = MValue<Native>;

        MCollection() =default;

#if DEBUG
        virtual
#endif
        ~MCollection() {
            release(_context);
        }

        void init(MValue *slot, MCollection *parent) {
            assert(slot);
            assert(!_context);
            _slot = slot;
            _parent = parent;
            if (_slot->value())
                _context = retain(_parent->_context);
        }

    public:
        void setSlot(MValue *newSlot, MValue *oldSlot) {
            if (_slot == oldSlot) {
                _slot = newSlot;
                if (!newSlot)
                    _parent = nullptr;
            }
        }

        MCollection* parent() const         {return _parent;}
        alloc_slice originalData() const    {return _context->_data;}
        SharedKeys* sharedKeys() const      {return _context->_sharedKeys;}
        bool mutableContainers() const      {return _context->_mutableContainers;}

    protected:
        bool isMutated() const {
            return !_slot || _slot->isMutated();
        }

        void mutate() {
            if (_slot)
                _slot->mutate();
            if (_parent)
                _parent->mutate();
        }

        // Only for use by MRoot
        MCollection(Context *context)
        :_context(retain(context))
        { }

    private:
        MValue*         _slot {nullptr};            // Value representing this collection
        MCollection*    _parent {nullptr};          // Parent collection
        Context*        _context {nullptr};         // Document data, sharedKeys, etc.
    };


    /** Top-level object; a type of special single-element Collection that contains the root. */
    template <class Native>
    class MRoot : private MCollection<Native> {
    public:
        using MCollection = MCollection<Native>;

        MRoot() =default;

        MRoot(alloc_slice fleeceData,
              SharedKeys *sk,
              const Value *value,
              bool mutableContainers =true)
        :MCollection(new Context(fleeceData, sk, mutableContainers))
        ,_slot(value)
        { }

        MRoot(alloc_slice fleeceData,
              SharedKeys *sk =nullptr,
              bool mutableContainers =true)
        :MRoot(fleeceData, sk, Value::fromData(fleeceData), mutableContainers)
        { }

        static Native asNative(alloc_slice fleeceData,
                               SharedKeys *sk =nullptr,
                               bool mutableContainers =true)
        {
            MRoot root(fleeceData, sk, mutableContainers);
            return root.asNative();
        }

        explicit operator bool() const      {return !_slot.isEmpty();}

        alloc_slice originalData() const    {return MCollection::originalData();}
        SharedKeys* sharedKeys() const      {return MCollection::sharedKeys();}

        Native asNative() const             {return _slot.asNative(this);}
        bool isMutated() const              {return _slot.isMutated();}
        void encodeTo(Encoder &enc) const   {_slot.encodeTo(enc);}

        alloc_slice encode() const          {Encoder enc; encodeTo(enc); return enc.extractOutput();}

        alloc_slice encodeDelta() const {
            Encoder enc;
            enc.setBase(originalData());
            enc.reuseBaseStrings();
            encodeTo(enc);
            return enc.extractOutput();
        }

    private:
        MValue<Native>  _slot;              // My contents: a holder for the actual root object
    };

}
