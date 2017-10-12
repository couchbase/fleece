//
//  MContext.cc
//  Fleece
//
//  Created by Jens Alfke on 10/5/17.
//Copyright © 2017 Couchbase. All rights reserved.
//

#include "MContext.hh"

namespace fleeceapi {

    MContext::MContext(const alloc_slice &data, FLSharedKeys sk)
    :_data(data)
    ,_sharedKeys(sk)
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
