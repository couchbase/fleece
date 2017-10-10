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

namespace fleeceapi {

    namespace internal {

        /** Fleece backing-store state shared between all MCollections based on it. */
        struct Context {
            Context(const alloc_slice &data, FLSharedKeys sk, bool mutableContainers);
            Context();
#ifndef NDEBUG
            ~Context();
            static std::atomic_int gInstanceCount;
#endif
            /** An empty context. (Clients point to this instead of nullptr.) */
            static Context* const gNullContext;

            alloc_slice      _data;                     // Fleece data; ensures it doesn't go away
            FLSharedKeys     _sharedKeys {nullptr};     // SharedKeys to use with Dicts
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
        Keeps a strong reference to a Context, and manages upward connections to slot & parent. */
    template <class Native>
    class MCollection {
    public:
        /** Returns true if the child containers in this collection should be mutable. */
        bool mutableContainers() const      {return _context->_mutableContainers;}

        /** Returns true if this collection or its contents (at any level) have been modified. */
        bool isMutated() const              {return _mutated;}

        /** The original Fleece data that the entire document was read from. */
        alloc_slice originalData() const    {return _context->_data;}

        /** The FLSharedKeys used for encoding dictionary keys. */
        FLSharedKeys sharedKeys() const     {return _context->_sharedKeys;}

        /** The parent collection, if any. */
        MCollection* parent() const         {return _parent;}

    protected:
        using MValue = MValue<Native>;

        MCollection()                               =default;
        MCollection(internal::Context *context)     :_context(internal::retain(context)) { }
        ~MCollection()                              {release(_context);}

        void initInSlot(MValue *slot, MCollection *parent) {
            assert(slot);
            assert(!_context);
            _slot = slot;
            _parent = parent;
            _mutated = _slot->isMutated();
            if (_slot->value())
                _context = internal::retain(_parent->_context);
        }

        void setSlot(MValue *newSlot, MValue *oldSlot) {
            if (_slot == oldSlot) {
                _slot = newSlot;
                if (!newSlot)
                    _parent = nullptr;
            }
        }

        internal::Context* context() const {
            return _context;
        }
        
        void setContext(internal::Context *ctx) {
            assert(!_context);
            _context = internal::retain(ctx);
        }

        void mutate() {
            if (!_mutated) {
                _mutated = true;
                if (_slot)
                    _slot->mutate();
                if (_parent)
                    _parent->mutate();
            }
        }

    private:
        MValue*             _slot {nullptr};        // Value representing this collection
        MCollection*        _parent {nullptr};      // Parent collection
        internal::Context*  _context {nullptr};     // Document data, sharedKeys, etc. NEVER NULL
        bool                _mutated {true};        // Has my value changed from the backing store?

        friend MValue;
    };

}
