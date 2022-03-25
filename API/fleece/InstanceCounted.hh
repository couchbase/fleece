//
// InstanceCounted.hh
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "fleece/function_ref.hh"
#include <cstddef> //for size_t
#include <atomic>
#include <stdint.h>

#ifndef INSTANCECOUNTED_TRACK
    // TODO: Add this guard back in
    //#if DEBUG
        #define INSTANCECOUNTED_TRACK 1
    //#endif
#endif

namespace fleece {

    /** Base class that keeps track of the total instance count of it and all subclasses.
        This is useful for leak detection.
        In debug builds or if INSTANCECOUNTED_TRACK is defined, the class will also track the
        individual instance addresses, which can be logged by calling `dumpInstances`. */
    class InstanceCounted {
    public:

        /** Total number of live objects that implement InstanceCounted. */
        static int liveInstanceCount()              {return gInstanceCount;}

#if INSTANCECOUNTED_TRACK
        InstanceCounted()                           {track();}
        InstanceCounted(const InstanceCounted&)     {track();}
        InstanceCounted(InstanceCounted &&old)      {track(); old.untrack();}
        virtual ~InstanceCounted()                  {untrack();}        // must be virtual for RTTI

        /** Logs information to stderr about all live objects. */
        static void dumpInstances()                                        {dumpInstances(nullptr);}
        static void dumpInstances(function_ref<void(const InstanceCounted*)> f) {dumpInstances(&f);}

    protected:
        InstanceCounted(size_t offset)              {track(offset);}
    private:
        void track(size_t offset =0) const;
        void untrack() const;
        static void dumpInstances(function_ref<void(const InstanceCounted*)>*);

#else
        InstanceCounted()                           {++gInstanceCount;}
        InstanceCounted(const InstanceCounted&)     {++gInstanceCount;}
        InstanceCounted(InstanceCounted &&old)      {} // Do nothing, the old and new should balance
        ~InstanceCounted()                          {--gInstanceCount;}
#endif

    private:
        static std::atomic<int> gInstanceCount;
    };


    /** Alternative to InstanceCounted that must be used in cases where
        - you're using multiple inheritance,
        - InstanceCounted is not the first parent class listed,
        - _and_ an earlier parent class has virtual methods.
        In that situation, InstanceCounted won't be able to determine the exact address of the
        object (due to the weird way C++ MI works), so instead you should use
        InstanceCountedIn<MyClass>, where MyClass is the class you're declaring. For example:
            class MyClass : public BaseClassWithVirtual, InstanceCountedIn<MyClass> { ... };
        */
    template <class BASE>
    class InstanceCountedIn : public InstanceCounted {
    public:
#if INSTANCECOUNTED_TRACK
        InstanceCountedIn()
        :InstanceCounted((size_t)this - (size_t)(BASE*)this)
        { }

        InstanceCountedIn(const InstanceCountedIn&)
        :InstanceCounted((size_t)this - (size_t)(BASE*)this)
        { }

        InstanceCountedIn(InstanceCountedIn &&old)
        :InstanceCounted((size_t)this - (size_t)(BASE*)this)
        {
            old.untrack();
        }
#endif
    };


}
