//
// MContext.hh
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "MValue.hh"
#include <atomic>

namespace fleece {

    /** Fleece backing-store state shared between all MCollections based on it.
        You can subclass this if there is other data you need to share across collections,
        or if the Fleece data is held in memory by something other than an alloc_slice. */
    class MContext {
    public:
        MContext(const alloc_slice &data);
        virtual ~MContext();

#ifndef NDEBUG
        static std::atomic_int gInstanceCount;
#endif

        /** The data of the Fleece document from which the root was loaded. */
        virtual slice data() const                  {return _data;}

        inline MContext* retain() {
            ++_refCount;
            return this;
        }

        inline void release() {
            if (_usuallyFalse(--_refCount == 0))
                delete this;
        }

        /** An empty context. (Clients point to this instead of nullptr.) */
        static MContext* const gNullContext;

    private:
        std::atomic_uint _refCount {0};             // Reference count
        alloc_slice      _data;                     // Fleece data; ensures it doesn't go away

    private:
        MContext();
    };

}
