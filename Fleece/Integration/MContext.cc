//
// MContext.cc
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

#include "MContext.hh"

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
