//
// RefCounted.cc
//
// Copyright (c) 2016 Couchbase, Inc All rights reserved.
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

#include "RefCounted.hh"
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>

namespace fleece {

    static void fail(RefCounted *obj, const char *what, int refCount) {
        char message[100];
        sprintf(message, "RefCounted object at %p %s while it had an invalid refCount of %d",
             obj, what, refCount);
#ifdef WarnError
        WarnError("%s", message);
#else
        fprintf(stderr, "WARNING: %s\n", message);
#endif
        throw std::runtime_error(message);
    }


    RefCounted::~RefCounted() {
        // Store a garbage value to detect use-after-free
        int32_t oldRef = _refCount.exchange(-9999999);
        if (oldRef != 0 && oldRef != kCarefulInitialRefCount) {
            // Detect if the destructor is not called from _release, i.e. the object still has
            // references. This is probably a direct call to delete, which is illegal.
            // Or possibly some other thread had a pointer to this object without a proper
            // reference, and then called retain() on it after this thread's release() set the
            // ref-count to 0.
            fail(this, "destructed", oldRef);
        }
    }


    // In debug builds, sanity-check the ref-count on retain and release. This can detect several
    // problems, like a corrupted object (garbage out-of-range refcount), or a race condition where
    // one thread releases the last reference to an object and destructs it, while simultaneously
    // another thread (that shouldn't have a reference but does due to a bug) retains or releases
    // the object.


    void RefCounted::_careful_retain() noexcept {
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


    void RefCounted::_careful_release() noexcept {
        auto oldRef = _refCount--;

        // If the refCount was 0 we have a bug where another thread is destructing
        // the object, so this thread shouldn't have a reference at all.
        // Or if the refcount was negative or ridiculously big, this is probably a garbage object.
        if (oldRef <= 0 || oldRef >= 10000000)
            fail(this, "released", oldRef);

        // If the refCount just went to 0, delete the object:
        if (oldRef == 1) delete this;
    }
    
}
