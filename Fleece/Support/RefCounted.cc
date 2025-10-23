//
// RefCounted.cc
//
// Copyright 2016-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "fleece/RefCounted.hh"
#include "AtomicRetained.hh"
#include "Backtrace.hh"
#include "betterassert.hh"
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#include "asprintf.h"
#endif

namespace fleece {

#if !DEBUG
    __hot void RefCounted::_release() const noexcept {
        if (--_refCount <= 0)
            delete this;
    }
#endif


    __hot void release(const RefCounted *r) noexcept {
        if (r) r->_release();
    }


    __hot void assignRef(RefCounted* &holder, RefCounted *newValue) noexcept {
        RefCounted *oldValue = holder;
        if (_usuallyTrue(newValue != oldValue)) {
            if (newValue) newValue->_retain();
            holder = newValue;
            if (oldValue) oldValue->_release();
        }
    }


    __cold static void fail(const RefCounted *obj, const char *what, int refCount,
                            bool andThrow =true)
    {
        char *message = nullptr;
        int n = asprintf(&message,
                 "RefCounted object <%s @ %p> %s while it had an invalid refCount of %d (0x%x)",
                 Unmangle(typeid(*obj)).c_str(), obj, what, refCount, unsigned(refCount));
        if (n < 0)
            throw std::runtime_error("RefCounted object has an invalid refCount");
#ifdef WarnError
        WarnError("%s", message);
#else
        fprintf(stderr, "WARNING: %s\n", message);
#endif
        if (andThrow) {
            std::string str(message);
            free(message);
            throw std::runtime_error(str);
        }
        free(message);
    }


    RefCounted::~RefCounted() noexcept {
        // Store a garbage value to detect use-after-free
        int32_t oldRef = _refCount.exchange(-9999999);
        if (_usuallyFalse(oldRef != 0)) {
#if DEBUG
            if (oldRef != kCarefulInitialRefCount)
#endif
            {
                // Detect if the destructor is not called from _release, i.e. the object still has
                // references. This is probably a direct call to delete, which is illegal.
                // Or possibly some other thread had a pointer to this object without a proper
                // reference, and then called retain() on it after this thread's release() set the
                // ref-count to 0.
                fail(this, "destructed", oldRef, false);
            }
        }
    }


    // In debug builds, sanity-check the ref-count on retain and release. This can detect several
    // problems, like a corrupted object (garbage out-of-range refcount), or a race condition where
    // one thread releases the last reference to an object and destructs it, while simultaneously
    // another thread (that shouldn't have a reference but does due to a bug) retains or releases
    // the object.


    void RefCounted::_careful_retain() const noexcept {
        auto oldRef = _refCount++;

        // Special case: the initial retain of a new object that takes it to refCount 1
        if (oldRef == kCarefulInitialRefCount) {
            _refCount = 1;
            return;
        }

        // Otherwise, if the refCount was 0 we have a bug where another thread is destructing
        // the object, so this thread shouldn't have a reference at all.
        // Or if the refcount was negative or ridiculously big, this is probably a garbage object.
        if (oldRef <= 0 || oldRef >= 10000000)
            fail(this, "retained", oldRef);
    }


    void RefCounted::_careful_release() const noexcept {
        auto oldRef = _refCount--;

        // If the refCount was 0 we have a bug where another thread is destructing
        // the object, so this thread shouldn't have a reference at all.
        // Or if the refcount was negative or ridiculously big, this is probably a garbage object.
        if (oldRef <= 0 || oldRef >= 10000000)
            fail(this, "released", oldRef);

        // If the refCount just went to 0, delete the object:
        if (oldRef == 1) delete this;
    }


    // Called by Retained<>. Broken out so it's not duplicated in each instantiaton of the template.
    __cold void _failNullRef() {
        throw std::invalid_argument("storing nullptr in a non-nullable Retained");
    }


    namespace internal {
        /// Tag bit that's added to `_ref` while accessing it.
        /// We can't use the low bit (1) because mutable Fleece Values already use that as a tag.
        static constexpr uintptr_t kBusyMask = uintptr_t(1) << 63;

        AtomicWrapper::AtomicWrapper(uintptr_t ref) noexcept
        :_ref(ref)
        {
            assert((ref & kBusyMask) == 0);
        }


        uintptr_t AtomicWrapper::exchangeWith(uintptr_t newRef) noexcept {
            uintptr_t oldRef = getAndLock();
            setAndUnlock(oldRef, newRef);
            return oldRef;
        }

        /// Loads and returns the value of `_ref`, while atomically setting `_ref`s high bit
        /// to mark it as busy. If the high bit is set, busy-waits until it's cleared.
        /// (The busy-wait should be OK since the high bit will only be set very briefly.)
        /// @warning This MUST be followed ASAP by `setAndUnlock`.
        uintptr_t AtomicWrapper::getAndLock() const noexcept {
            uintptr_t r = _ref.load(std::memory_order_acquire);
            while (true) {
                if (r & kBusyMask)
                    r = _ref.load(std::memory_order_acquire);
                else if (_ref.compare_exchange_strong(r, r | kBusyMask, std::memory_order_acquire))
                    break;
            }
            assert((r & kBusyMask) == 0);
            return r;
        }

        /// Changes `_ref`s value from `oldRef` to `newRef`, clearing the busy bit.
        /// MUST only be called after `getAndLock`.
        void AtomicWrapper::setAndUnlock(uintptr_t oldRef, uintptr_t newRef) const noexcept {
            uintptr_t r = oldRef | kBusyMask;
            bool ok = _ref.compare_exchange_strong(r, newRef, std::memory_order_release);
            assert(ok);
        }
    }

}
