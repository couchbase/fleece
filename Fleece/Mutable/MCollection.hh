//
//  MCollection.hh
//  Fleece
//
//  Created by Jens Alfke on 10/4/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "MValue.hh"
#include "MContext.hh"

namespace fleeceapi {

    /** Abstract superclass of MArray and MDict.
        Keeps a strong reference to a Context, and manages upward connections to slot & parent. */
    template <class Native>
    class MCollection {
    public:
        /** Returns true if this collection or its contents (at any level) have been modified. */
        bool isMutable() const              {return _mutable;}

        /** Returns true if this collection or its contents (at any level) have been modified. */
        bool isMutated() const              {return _mutated;}

        /** Returns true if the child containers in this collection should be mutable. */
        bool mutableChildren() const        {return _mutableChildren;}

        void setMutableChildren(bool m)     {assert(_mutable); _mutableChildren = m;}

        /** The shared context of the object tree. */
        MContext* context() const           {return _context;}

        /** The parent collection, if any. */
        MCollection* parent() const         {return _parent;}

    protected:
        using MValue = MValue<Native>;

        MCollection()                               =default;

        MCollection(MContext *context, bool isMutable)
        :_context(context->retain())
        ,_mutable(isMutable)
        ,_mutableChildren(isMutable)
        { }

        ~MCollection()                              {if (_context) _context->release();}

        void initInSlot(MValue *slot, MCollection *parent, bool isMutable) {
            assert(slot);
            assert(!_context);
            _slot = slot;
            _parent = parent;
            _mutable = isMutable;
            _mutableChildren = isMutable;
            _mutated = _slot->isMutated();
            if (_slot->value())
                _context = _parent->_context->retain();
        }

        void initAsCopyOf(const MCollection &original, bool isMutable) {
            setContext(original._context);
            _mutable = _mutableChildren = isMutable;
        }

        void setSlot(MValue *newSlot, MValue *oldSlot) {
            if (_slot == oldSlot) {
                _slot = newSlot;
                if (!newSlot)
                    _parent = nullptr;
            }
        }

        void setContext(MContext *ctx) {
            assert(!_context);
            _context = ctx->retain();
        }

        void mutate() {
            assert(_mutable);
            if (!_mutated) {
                _mutated = true;
                if (_slot)
                    _slot->mutate();
                if (_parent)
                    _parent->mutate();
            }
        }

    private:
        MValue*         _slot {nullptr};            // Value representing this collection
        MCollection*    _parent {nullptr};          // Parent collection
        MContext*       _context {nullptr};         // Document data, sharedKeys, etc. NEVER NULL
        bool            _mutable {true};            // Am I mutable?
        bool            _mutated {true};            // Has my value changed from the backing store?
        bool            _mutableChildren {true};    // Should child containers be mutable?

        friend MValue;
    };

}
