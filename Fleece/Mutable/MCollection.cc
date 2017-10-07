//
//  MCollection.cc
//  Fleece
//
//  Created by Jens Alfke on 10/5/17.
//Copyright © 2017 Couchbase. All rights reserved.
//

#include "MCollection.hh"

namespace fleeceapi {
    using namespace internal;

    Context::Context(const alloc_slice &data, FLSharedKeys sk, bool mutableContainers)
    :_data(data)
    ,_sharedKeys(sk)
    ,_mutableContainers(mutableContainers)
    {
        //std::cerr << "INIT Context " << this << "\n";
#ifndef NDEBUG
        ++gInstanceCount;
#endif
    }


#ifndef NDEBUG
    std::atomic_int Context::gInstanceCount;


    Context::~Context() {
        assert(this != gNullContext);
        --gInstanceCount;
        //std::cerr << "DTOR Context " << this << "\n";
    }
#endif


    Context::Context()
    :_refCount(0x7FFFFFFF)
    { }

    Context* const Context::gNullContext = new Context;


}
