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
#include "fleece/WeakRef.hh"
#include "Backtrace.hh"
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#include "asprintf.h"
#endif

namespace fleece {

    static std::atomic<size_t> sZombieCount = 0;

    static void fail(const RefCounted *obj, const char *what, uint64_t refCount, bool andThrow =true);


    static uint32_t refCountOf(uint64_t rc) {
        return uint32_t(rc);
    }
    static uint32_t weakCountOf(uint64_t rc) {
        return uint32_t(rc >> 32);
    }
    #if DEBUG
    static bool invalidCount(uint32_t count) {
        return count >= 0x80000000;
    }
    #endif


    int RefCounted::refCount() const {
        return std::max(refCountOf(_bothRefCounts), 1u) - 1;  // Don't expose the internal 1 extra ref
    }


    std::pair<int, int> RefCounted::refCounts() const {
        uint64_t both = _bothRefCounts;
        return {std::max(refCountOf(both), 1u) - 1, weakCountOf(both)};
    }


    size_t RefCounted::zombieCount() {
        return sZombieCount;
    }


    // In debug builds, sanity-check the ref-count on retain and release. This can detect several
    // problems, like a corrupted object (garbage out-of-range refcount), or a race condition where
    // one thread releases the last reference to an object and destructs it, while simultaneously
    // another thread (that shouldn't have a reference but does due to a bug) retains or releases
    // the object.


    #if DEBUG
    void RefCounted::_retain() const noexcept {
        auto oldRef = _bothRefCounts++;

        // Special case: the initial retain of a new object that takes it to refCount 1
        if (oldRef == kInitialRefCount) {
            _bothRefCounts = 2;
            return;
        }

        // Otherwise, if the refCount was 0 we have a bug where another thread is destructing
        // the object, so this thread shouldn't have a reference at all.
        // Or if the refcount was negative or ridiculously big, this is probably a garbage object.
        if (refCountOf(oldRef) <= 1 || refCountOf(oldRef) >= 0x7FFFFFFF)
            fail(this, "retained", oldRef);
    }
#endif


    __hot void RefCounted::_release() const noexcept {
        auto newRef = --_bothRefCounts;
        if (refCountOf(newRef) == 1) {
            if (weakCountOf(newRef) == 0) {
                delete this;
            } else {
                this->~RefCounted();        // Weak references remain, so don't delete yet
                ++sZombieCount;
                if (--_bothRefCounts == 0) {
                    operator delete(const_cast<RefCounted*>(this));
                    --sZombieCount;
                }
            }
        }
#if DEBUG
        else if (refCountOf(newRef) == 0 || invalidCount(refCountOf(newRef)))
            fail(this, "released", newRef + 1);
#endif
    }


    void RefCounted::_weakRetain() const noexcept {
        auto newRef = _bothRefCounts += (1ull << 32);
#if DEBUG
        if (weakCountOf(newRef) < 1 || invalidCount(weakCountOf(newRef)))
            fail(this, "weakRetained", newRef - (1ull << 32));
#endif
    }


    void RefCounted::_weakRelease() const noexcept {
        auto newRef = _bothRefCounts -= (1ull << 32);
        if (weakCountOf(newRef) == 0) {
            if (refCountOf(newRef) == 0) {
                operator delete(const_cast<RefCounted*>(this));
                --sZombieCount;
            }
        }
#if DEBUG
        else if (invalidCount(weakCountOf(newRef)))
            fail(this, "weakReleased", newRef + (1ull << 32));
#endif
    }


    bool RefCounted::_weakToStrong() const noexcept {
        auto refs = _bothRefCounts.load();
        do {
            #if DEBUG
            if (weakCountOf(refs) == 0 || invalidCount(weakCountOf(refs)) || invalidCount(refCountOf(refs)))
                fail(this, "weakToStrong", refs);
            #endif
            if (refCountOf(refs) <= 1)
                return false;   // No strong refs; fail
        } while (!_bothRefCounts.compare_exchange_strong(refs, refs + 1));
        // Successfully added a strong ref; successs
        return true;
    }


    RefCounted::~RefCounted() noexcept {
        if (auto refs = _bothRefCounts.load(); refCountOf(refs) != 1) {
#if DEBUG
            if (refCountOf(refs) != kInitialRefCount)
#endif
            {
                // Detect if the destructor is not called from _release, i.e. the object still has
                // references. This is probably a direct call to delete, which is illegal.
                // Or possibly some other thread had a pointer to this object without a proper
                // reference, and then called retain() on it after this thread's release() set the
                // ref-count to 0.
                fail(this, "destructed", refs, false);
            }
        }
    }


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


    // Called by Retained<>. Broken out so it's not duplicated in each instantiaton of the template.
    __cold void _failNullRef() {
        throw std::invalid_argument("storing nullptr in a non-nullable Retained");
    }

    __cold void _failZombie(void* zombie) {
        throw std::invalid_argument("storing nullptr in a non-nullable Retained");
    }


    __cold static void fail(const RefCounted *obj, const char *what, uint64_t refCount, bool andThrow) {
        char *message = nullptr;
        int n = asprintf(&message,
                 "RefCounted object <%s @ %p> %s while it had an invalid refCount of 0x%llx",
                 Unmangle(typeid(*obj)).c_str(), obj, what, (unsigned long long)refCount);
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


}
