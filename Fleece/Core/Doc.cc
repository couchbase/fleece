//
// Doc.cc
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

#include "Doc.hh"
#include "SharedKeys.hh"
#include "Pointer.hh"
#include "JSONConverter.hh"
#include "FleeceException.hh"
#include "MutableDict.hh"
#include "MutableArray.hh"
#include <algorithm>
#include <functional>
#include <mutex>
#include <vector>
#include "betterassert.hh"

#if 0
#define Log(FMT,...) fprintf(stderr, "DOC: " # FMT "\n", __VA_ARGS__)
#else
#define Log(FMT,...)
#endif

#define Warn(FMT,...) fprintf(stderr, "DOC: WARNING: " # FMT "\n", __VA_ARGS__)


namespace fleece { namespace impl {
    using namespace std;
    using namespace internal;


    // `sMemoryMap` is a global mapping from pointers to Scopes.
    struct memEntry {
        const void *endOfRange; // The _end_ of the memory range covered by the Scope
        Scope *scope;           // The Scope
        bool operator< (const memEntry &other) const        {return endOfRange < other.endOfRange;}
    };

    using memoryMap = smallVector<memEntry, 10>;

    static memoryMap *sMemoryMap;

    // Mutex for access to `sMemoryMap`
    static mutex sMutex;


    Scope::Scope(slice data, SharedKeys *sk, slice destination) noexcept
    :_sk(sk)
    ,_externDestination(destination)
    ,_data(data)
    {
        registr();
    }


    Scope::Scope(const alloc_slice &data, SharedKeys *sk, slice destination) noexcept
    :_sk(sk)
    ,_externDestination(destination)
    ,_data(data)
    ,_alloced(data)
    {
        registr();
    }


    Scope::Scope(const Scope &parentScope, slice subData) noexcept
    :_sk(parentScope.sharedKeys())
    ,_externDestination(parentScope.externDestination())
    ,_data(subData)
    ,_alloced(parentScope._alloced)
    {
        // This ctor does _not_ register the data range, because the parent scope already did.
        _unregistered.test_and_set();
        if (subData)
            assert_precondition(parentScope.data().containsAddressRange(subData));
    }


    Scope::~Scope() {
        unregister();
    }


    __hot void Scope::registr() noexcept {
        _unregistered.test_and_set();
        if (!_data)
            return;

#if DEBUG
        if (_data.size < 1e6)
            _dataHash = _data.hash();
#endif
        lock_guard<mutex> lock(sMutex);
        if (_usuallyFalse(!sMemoryMap))
            sMemoryMap = new memoryMap;
        Log("Register   (%p ... %p) --> Scope %p, sk=%p [Now %zu]",
            _data.buf, _data.end(), this, _sk.get(), sMemoryMap->size()+1);

        if (!_isDoc && _data.size == 2) {
            // Values of size 2 are simple values in that they don't have sub-values. Therefore, they don't provide
            // interesting scope. An exception is the empty dict. We use the empty dict for empty revision bodies
            // of databases and apply technical check, c.f. DatabaseImpl::validateRevisionBody().
            // We are mostly concerned of the pre-encoded size-2 values, such as Value::kTrueValue, etc. Their address
            // ranges are constant, regardless of the associated scope, causing the failure of the assertion of,
            // "Incompatible duplicate Scope." CBSE-10529.

            // However, we should *always* register the scope if this is a Doc.

            if (auto t = ((const Value*)_data.buf)->type(); t != kDict) {
                return;
            }
        }

        memEntry entry = {_data.end(), this};
        memoryMap::iterator iter = upper_bound(sMemoryMap->begin(), sMemoryMap->end(), entry);

        // Assert that there isn't another conflicting Scope registered for this data:
        if (iter != sMemoryMap->begin() && prev(iter)->endOfRange == entry.endOfRange) {
            Scope *existing = prev(iter)->scope;
            if (existing->_data == _data && existing->_externDestination == _externDestination
                && existing->_sk == _sk) {
                Log("Duplicate  (%p ... %p) --> Scope %p, sk=%p",
                    _data.buf, _data.end(), this, _sk.get());
            } else {
                static const char* const valueTypeNames[] {"Null", "Boolean", "Number", "String", "Data", "Array", "Dict"};
                auto type1 = ((const Value*)_data.buf)->type();
                auto type2 = ((const Value*)existing->_data.buf)->type();
                FleeceException::_throw(InternalError,
                    "Incompatible duplicate Scope %p (%s) for (%p .. %p) with sk=%p: "
                    "conflicts with %p (%s) for (%p .. %p) with sk=%p",
                    this, valueTypeNames[type1], _data.buf, _data.end(), _sk.get(),
                    existing, valueTypeNames[type2], existing->_data.buf, existing->_data.end(),
                    existing->_sk.get());
            }
        }

        sMemoryMap->insert(iter, entry);
        _unregistered.clear();
    }


    __hot void Scope::unregister() noexcept {
        if (!_unregistered.test_and_set()) {            // this is atomic
#if DEBUG
            // Assert that the data hasn't been changed since I was created:
            if (_data.size < 1e6 && _data.hash() != _dataHash)
                FleeceException::_throw(InternalError,
                    "Memory range (%p .. %p) was altered while Scope %p (sk=%p) was active. "
                    "This usually means the Scope's data was freed/invalidated before the Scope "
                    "was unregistered/deleted. Unregister it earlier!",
                    _data.buf, _data.end(), this, _sk.get());
#endif

            lock_guard<mutex> lock(sMutex);
            Log("Unregister (%p ... %p) --> Scope %p, sk=%p   [now %zu]",
                _data.buf, _data.end(), this, _sk.get(), sMemoryMap->size()-1);
            memEntry entry = {_data.end(), this};
            auto iter = lower_bound(sMemoryMap->begin(), sMemoryMap->end(), entry);
            while (iter != sMemoryMap->end() && iter->endOfRange == entry.endOfRange) {
                if (iter->scope == this) {
                    sMemoryMap->erase(iter);
                    return;
                } else {
                    ++iter;
                }
            }
            Warn("unregister(%p) couldn't find an entry for (%p ... %p)", this, _data.buf, _data.end());
        }
    }


    __hot static const Value* resolveMutable(const Value *value) {
        if (_usuallyFalse(value->isMutable())) {
            // Scope doesn't know about mutable Values (they're in the heap), but the mutable
            // Value may be a mutable copy of a Value with scope...
            if (value->asDict())
                value = value->asDict()->asMutable()->source();
            else
                value = value->asArray()->asMutable()->source();
        }
        return value;
    }


    /*static*/ __hot const Scope* Scope::_containing(const Value *src) noexcept {
        // must have sMutex to call this
        if (_usuallyFalse(!sMemoryMap))
            return nullptr;
        auto iter = upper_bound(sMemoryMap->begin(), sMemoryMap->end(), memEntry{src, nullptr});
        if (_usuallyFalse(iter == sMemoryMap->end()))
            return nullptr;
        Scope *scope = iter->scope;
        if (_usuallyFalse(src < scope->_data.buf))
            return nullptr;
        return scope;
    }


    /*static*/ __hot const Scope* Scope::containing(const Value *v) noexcept {
        v = resolveMutable(v);
        if (!v)
            return nullptr;
        lock_guard<mutex> lock(sMutex);
        return _containing(v);
    }


    /*static*/ __hot SharedKeys* Scope::sharedKeys(const Value *v) noexcept {
        lock_guard<mutex> lock(sMutex);
        auto scope = _containing(v);
        return scope ? scope->sharedKeys() : nullptr;
    }


    const Value* Scope::resolveExternPointerTo(const void* dst) const noexcept {
        dst = offsetby(dst, (char*)_externDestination.end() - (char*)_data.buf);
        if (_usuallyFalse(!_externDestination.containsAddress(dst)))
            return nullptr;
        return (const Value*)dst;
    }


    /*static*/ const Value* Scope::resolvePointerFrom(const internal::Pointer* src,
                                                      const void *dst) noexcept
    {
        lock_guard<mutex> lock(sMutex);
        auto scope = _containing((const Value*)src);
        return scope ? scope->resolveExternPointerTo(dst) : nullptr;
    }


    /*static*/ pair<const Value*,slice> Scope::resolvePointerFromWithRange(const Pointer* src,
                                                                         const void* dst) noexcept
    {
        lock_guard<mutex> lock(sMutex);
        auto scope = _containing((const Value*)src);
        if (!scope)
            return { };
        return {scope->resolveExternPointerTo(dst), scope->externDestination()};
    }


    void Scope::dumpAll() {
        lock_guard<mutex> lock(sMutex);
        if (_usuallyFalse(!sMemoryMap)) {
            fprintf(stderr, "No Scopes have ever been registered.\n");
            return;
        }
        for (auto &entry : *sMemoryMap) {
            auto scope = entry.scope;
            fprintf(stderr, "%p -- %p (%4zu bytes) --> SharedKeys[%p]%s\n",
                    scope->_data.buf, scope->_data.end(), scope->_data.size, scope->sharedKeys(),
                    (scope->_isDoc ? " (Doc)" : ""));
        }
    }


#pragma mark - DOC:


    Doc::Doc(const alloc_slice &data, Trust trust, SharedKeys *sk, slice destination) noexcept
    :Scope(data, sk, destination)
    {
        init(trust);
    }


    Doc::Doc(const Doc *parentDoc, slice subData, Trust trust) noexcept
    :Scope(*parentDoc, subData)
    ,_parent(parentDoc)                         // Ensure parent is retained
    {
        init(trust);
    }


    Doc::Doc(const Scope &parentScope, slice subData, Trust trust) noexcept
    :Scope(parentScope, subData)
    {
        init(trust);
    }

    void Doc::init(Trust trust) noexcept {
        if (data() && trust != kDontParse) {
            _root = trust ? Value::fromTrustedData(data()) : Value::fromData(data());
            if (!_root)
                unregister();
        }
        _isDoc = true;
    }


    Retained<Doc> Doc::fromFleece(const alloc_slice &fleece, Trust trust) {
        return new Doc(fleece, trust);
    }

    Retained<Doc> Doc::fromJSON(slice json, SharedKeys *sk) {
        return new Doc(JSONConverter::convertJSON(json, sk), kTrusted, sk);
    }


    /*static*/ RetainedConst<Doc> Doc::containing(const Value *src) noexcept {
        src = resolveMutable(src);
        if (!src)
            return nullptr;
        lock_guard<mutex> lock(sMutex);
        const Scope *scope = _containing(src);
        if (!scope)
            return nullptr;
        assert_postcondition(scope->_isDoc);
        return RetainedConst<Doc>((const Doc*)scope);
    }

} }


void FLDumpScopes() {
    fleece::impl::Scope::dumpAll();
}
