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
    #if DEBUG
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

#if DEBUG
        virtual
#endif
        ~MCollection()                      {release(_context);}

        void init(MValue *slot, MCollection *parent) {
            assert(slot);
            assert(!_context);
            _slot = slot;
            _parent = parent;
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

        bool isMutated() const              {return !_slot || _slot->isMutated();}

    protected:
        void mutate() {
            if (_slot)
                _slot->mutate();
            if (_parent)
                _parent->mutate();
        }

        // Only for use by MRoot
        MCollection(internal::Context *context)     :_context(internal::retain(context)) { }

    private:
        MValue*             _slot {nullptr};        // Value representing this collection
        MCollection*        _parent {nullptr};      // Parent collection
        internal::Context*  _context {nullptr};     // Document data, sharedKeys, etc.
    };

}
