//
// MContext.cc
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "MContext.hh"
#include "betterassert.hh"

namespace fleece {

    MContext::MContext(const alloc_slice &data)
    :_data(data)
    {
#ifndef NDEBUG
        ++gInstanceCount;
#endif
    }


#ifndef NDEBUG
    std::atomic_int MContext::gInstanceCount;
#endif


    MContext::~MContext() {
        assert(this != gNullContext);
#ifndef NDEBUG
        --gInstanceCount;
#endif
    }


    MContext::MContext()
    :_refCount(0x7FFFFFFF)
    { }

    MContext* const MContext::gNullContext = new MContext;

}
