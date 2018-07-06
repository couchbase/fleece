//
// ExternResolver.cc
//
// Copyright © 2018 Couchbase. All rights reserved.
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

#include "ExternResolver.hh"
#include "Value.hh"
#include <functional>
#include <mutex>
#include <set>

namespace fleece {
    using namespace std;

    static mutex sMutex;
    static map<size_t, ExternResolver*>* sMemoryMap;


    ExternResolver::ExternResolver(slice document, slice destination)
    :_document(document)
    ,_destinationDoc(destination)
    {
        lock_guard<mutex> lock(sMutex);
        if (!sMemoryMap)
            sMemoryMap = new map<size_t, ExternResolver*>;
        sMemoryMap->insert({size_t(document.end()), this});
    }

    ExternResolver::~ExternResolver() {
        lock_guard<mutex> lock(sMutex);
        sMemoryMap->erase(size_t(_document.end()));
    }


    const Value* ExternResolver::resolvePointerTo(const void* dst) const {
        dst = offsetby(dst, (char*)_destinationDoc.end() - (char*)_document.buf);
        if (!_destinationDoc.contains(dst))
            return nullptr;
        return (const Value*)dst;
    }

    /*static*/ const ExternResolver* ExternResolver::resolverForPointerFrom(const void *src) {
        lock_guard<mutex> lock(sMutex);
        if (!sMemoryMap)
            return nullptr;
        auto i = sMemoryMap->upper_bound(size_t(src));
        if (i == sMemoryMap->end())
            return nullptr;
        ExternResolver *resolver = i->second;
        if (src < resolver->_document.buf)
            return nullptr;
        return resolver;
    }

    /*static*/ const Value* ExternResolver::resolvePointerFrom(const void* src, const void *dst) {
        auto resolver = resolverForPointerFrom(src);
        return resolver ? resolver->resolvePointerTo(dst) : nullptr;
    }

}
