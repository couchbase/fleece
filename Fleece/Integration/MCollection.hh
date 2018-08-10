//
// MCollection.hh
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
#include "MValue.hh"
#include "MContext.hh"

namespace fleece {

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

        MCollection()
        :MCollection(MContext::gNullContext, true)
        { }

        MCollection(MContext *context, bool isMutable)
        :_context(context->retain())
        ,_mutable(isMutable)
        ,_mutableChildren(isMutable)
        { }

        ~MCollection() {
            _context->release();
        }

        void initInSlot(MValue *slot, MCollection *parent, bool isMutable) {
            assert(slot);
            assert(_context == MContext::gNullContext);
            _slot = slot;
            _parent = parent;
            _mutable = isMutable;
            _mutableChildren = isMutable;
            _mutated = _slot->isMutated();
            if (_slot->value())
                setContext(_parent->_context);
        }

        void initAsCopyOf(const MCollection &original, bool isMutable) {
            assert(_context == MContext::gNullContext);
            setContext(original._context);
            _mutable = _mutableChildren = isMutable;
        }

        void setSlot(MValue *newSlot, MValue *oldSlot) {
            if (_usuallyTrue(_slot == oldSlot)) {
                _slot = newSlot;
                if (!newSlot)
                    _parent = nullptr;
            }
        }

        void setContext(MContext *ctx) {
            if (_usuallyTrue(ctx != _context)) {
                _context->release();
                _context = ctx->retain();
            }
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
        MCollection(const MCollection&) =delete;
        MCollection& operator= (const MCollection &) =delete;

        MValue*         _slot {nullptr};            // Value representing this collection
        MCollection*    _parent {nullptr};          // Parent collection
        MContext*       _context {nullptr};         // Document data, sharedKeys, etc. NEVER NULL
        bool            _mutable {true};            // Am I mutable?
        bool            _mutated {true};            // Has my value changed from the backing store?
        bool            _mutableChildren {true};    // Should child containers be mutable?

        friend MValue;
    };

}
