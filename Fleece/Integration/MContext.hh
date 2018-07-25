//
// MContext.hh
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
#include "PlatformCompat.hh"
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
            if (_usuallyFalse(--_refCount == 0))
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
