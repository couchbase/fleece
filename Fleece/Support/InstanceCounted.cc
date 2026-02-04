//
// InstanceCounted.cc
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "fleece/InstanceCounted.hh"
#include "fleece/RefCounted.hh"
#include <map>
#include <mutex>
#include <stdlib.h>
#include <stdio.h>
#include <typeinfo>

#if __has_include(<cxxabi.h>)
#include <cxxabi.h>
#endif

namespace fleece {
    using namespace std;

    std::atomic<int> InstanceCounted::gInstanceCount;

// LCOV_EXCL_START
#if INSTANCECOUNTED_TRACK
    static mutex sInstancesMutex;
    static map<const InstanceCounted*,size_t> sInstances;

    void InstanceCounted::track(size_t offset) const {
        // `offset` is the offset from `this` (the InstanceCounted instance) to the main object
        // it's part of. This is stored in the map so it can be used to log the address of the main
        // object, which is the one we really care about.
        lock_guard<mutex> lock(sInstancesMutex);
        sInstances.insert({this, offset});
        ++gInstanceCount;
    }

    void InstanceCounted::untrack() const {
        lock_guard<mutex> lock(sInstancesMutex);
        sInstances.erase(this);
        --gInstanceCount;
    }

    void InstanceCounted::dumpInstances(function_ref<void(const InstanceCounted*)> *callback) {
        char* unmangled = nullptr;
        lock_guard<mutex> lock(sInstancesMutex);
        for (auto entry : sInstances) {
            const char *name =  typeid(*entry.first).name();
            const void *address = (uint8_t*)entry.first - entry.second;
    #if __has_include(<cxxabi.h>)
            int status;
            size_t unmangledLen = 0;
            unmangled = abi::__cxa_demangle(name, unmangled, &unmangledLen, &status);
            if (unmangled && status == 0)
                name = unmangled;
    #endif

            fprintf(stderr, "    * ");
            if (callback)
                (*callback)(entry.first);
            fprintf(stderr, "%s ", name);
            if (auto rc = dynamic_cast<const RefCounted*>(entry.first); rc)
                fprintf(stderr, "(refCount=%d) ", rc->refCount());
            fprintf(stderr, "at %p", address);
            if (!callback) {
                fprintf(stderr, "[");
                for (int i=0; i < 4; i++) {
                    if (i > 0)
                        fprintf(stderr, " ");
                    fprintf(stderr, "%08x", ((uint32_t*)address)[i]);
                }
                fprintf(stderr, "]");
            }
            fprintf(stderr, "\n");
        }
        free(unmangled);
    }

#endif // INSTANCECOUNTED_TRACK
// LCOV_EXCL_STOP


}
