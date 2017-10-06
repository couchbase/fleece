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

    namespace internal {
        /** Fleece backing-store state shared between all MCollections based on it. */
        struct Context {
            Context(const alloc_slice &data, SharedKeys *sk, bool mutableContainers);
            Context();
    #ifndef NDEBUG
            virtual ~Context();
            static std::atomic_int gInstanceCount;
    #endif
            static Context* const gNullContext;

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
    }


    /** Abstract superclass of MArray and MDict.
        Keeps a Context, and manages upward connections to slot & parent. */
    template <class Native>
    class MCollection {
    protected:
        using MValue = MValue<Native>;

        MCollection() =default;

#ifndef NDEBUG
        virtual
#endif
        ~MCollection()                      {release(_context);}

        void initInSlot(MValue *slot, MCollection *parent) {
            assert(slot);
            assert(!_context);
            _slot = slot;
            _parent = parent;
            _mutated = _slot->isMutated();
            if (_slot->value())
                _context = internal::retain(_parent->_context);
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

        bool isMutated() const              {return _mutated;}

    protected:
        void mutate() {
            if (!_mutated) {
                _mutated = true;
                if (_slot)
                    _slot->mutate();
                if (_parent)
                    _parent->mutate();
            }
        }

        // Only for use by MRoot
        MCollection(internal::Context *context)     :_context(internal::retain(context)) { }

    private:
        MValue*             _slot {nullptr};        // Value representing this collection
        MCollection*        _parent {nullptr};      // Parent collection
        internal::Context*  _context {nullptr};     // Document data, sharedKeys, etc.
        bool                _mutated {true};        // Has my value changed from the backing store?
    };

}
