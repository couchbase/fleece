//
//  MContext.hh
//  Fleece
//
//  Created by Jens Alfke on 10/10/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "MValue.hh"
#include <atomic>

namespace fleeceapi {

    /** Fleece backing-store state shared between all MCollections based on it.
        You can subclass this if there is other data you need to share across collections,
        or if the Fleece data is held in memory by something other than an alloc_slice. */
    class MContext {
    public:
        MContext(const alloc_slice &data, FLSharedKeys sk);
        virtual ~MContext();

#ifndef NDEBUG
        static std::atomic_int gInstanceCount;
#endif

        /** The data of the Fleece document from which the root was loaded. */
        virtual slice data() const                  {return _data;}

        /** The shared keys used to encode dictionary keys. */
        virtual FLSharedKeys sharedKeys() const     {return _sharedKeys;}

        inline MContext* retain() {
            ++_refCount;
            return this;
        }

        inline void release() {
            if (--_refCount == 0)
                delete this;
        }

        /** An empty context. (Clients point to this instead of nullptr.) */
        static MContext* const gNullContext;

    private:
        std::atomic_uint _refCount {0};             // Reference count
        alloc_slice      _data;                     // Fleece data; ensures it doesn't go away
        FLSharedKeys     _sharedKeys {nullptr};     // SharedKeys to use with Dicts

    private:
        MContext();
    };

}
