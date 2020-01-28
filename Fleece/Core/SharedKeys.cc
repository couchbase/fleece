//
// SharedKeys.cc
//
// Copyright (c) 2016 Couchbase, Inc All rights reserved.
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

#include "SharedKeys.hh"
#include "FleeceImpl.hh"
#include "FleeceException.hh"


#define LOCK(MUTEX)     lock_guard<mutex> _lock(const_cast<mutex&>(MUTEX))


namespace fleece { namespace impl {
    using namespace std;


    SharedKeys::~SharedKeys() {
    #ifdef __APPLE__
        for (auto &str : _platformStringsByKey) {
            if (str)
                CFRelease(str);
        }
    #endif
    }

    key_t::key_t(const Value *v) noexcept {
        if (v->isInteger())
            _int = (int16_t)v->asInt();
        else
            _string = v->asString();
    }

    bool key_t::operator== (const key_t &k) const noexcept {
        return shared() ? (_int == k._int) : (_string == k._string);
    }

    bool key_t::operator< (const key_t &k) const noexcept {
        if (shared())
            return k.shared() ? (_int < k._int) : true;
        else
            return k.shared() ? false : (_string < k._string);
    }



    bool SharedKeys::loadFrom(slice stateData) {
        const Value *v = Value::fromData(stateData);
        if (!v)
            return false;
        const Array *strs = v->asArray();
        if (!strs)
            return false;

        Array::iterator i(strs);
        if (i.count() <= count())
            return false;

        LOCK(_mutex);
        i += (unsigned)count();           // Start at the first _new_ string
        for (; i; ++i) {
            slice str = i.value()->asString();
            if (!str)
                return false;
            SharedKeys::_add(str);
        }
        return true;
    }


    alloc_slice SharedKeys::stateData() const {
        auto count = _count;
        Encoder enc;
        enc.beginArray(count);
        for (size_t key = 0; key < count; ++key)
            enc.writeString(_byKey[key]);
        enc.endArray();
        return enc.finish();
    }


    bool SharedKeys::encode(slice str, int &key) const {
        LOCK(_mutex);
        return _encode(str, key);
    }

    bool SharedKeys::_encode(slice str, int &key) const {
        // Is this string already encoded?
        auto &slot = _table.find(str);
        if (_usuallyTrue(slot.first.buf != nullptr)) {
            key = slot.second.offset;
            return true;
        }
        return false;
    }


    bool SharedKeys::encodeAndAdd(slice str, int &key) {
        LOCK(_mutex);
        return _encodeAndAdd(str, key);
    }

    bool SharedKeys::_encodeAndAdd(slice str, int &key) {
        if (_encode(str, key))
            return true;
        // Should this string be encoded?
        if (!couldAdd(str))
            return false;
        // OK, add to table:
        key = _add(str);
        return true;
    }


    bool SharedKeys::isEligibleToEncode(slice str) const {
        for (size_t i = 0; i < str.size; ++i)
            if (_usuallyFalse(!isalnum(str[i]) && str[i] != '_' && str[i] != '-'))
                return false;
        return true;
    }


    slice SharedKeys::decodeUnknown(int key) const {
        throwIf(key < 0, InvalidData, "key must be non-negative");
        // Unrecognized key -- if not in a transaction, try reloading
        const_cast<SharedKeys*>(this)->refresh();
        if (isUnknownKey(key))
            return nullslice;
        return _byKey[key];
    }


    vector<alloc_slice> SharedKeys::byKey() const {
        auto count = _count;
        return vector<alloc_slice>(&_byKey[0], &_byKey[count]);
    }


    SharedKeys::PlatformString SharedKeys::platformStringForKey(int key) const {
        throwIf(key < 0, InvalidData, "key must be non-negative");
        LOCK(_mutex);
        if ((unsigned)key >= _platformStringsByKey.size())
            return nullptr;
        return _platformStringsByKey[key];
    }


    void SharedKeys::setPlatformStringForKey(int key, SharedKeys::PlatformString platformKey) const {
        throwIf(key < 0, InvalidData, "key must be non-negative");
        throwIf((unsigned)key >= _count, InvalidData, "key is not yet known");
        LOCK(_mutex);
        auto &strings = const_cast<SharedKeys*>(this)->_platformStringsByKey;
        if ((unsigned)key >= _platformStringsByKey.size())
            strings.resize(key + 1);
        
        #ifdef __APPLE__
                strings[key] = CFStringCreateCopy(kCFAllocatorDefault, platformKey);
        #else
                strings[key] = platformKey;
        #endif
    }


    int SharedKeys::_add(slice str) {
        alloc_slice allocedStr(str);
        auto id = _count++;
        _byKey[id] = allocedStr;
        StringTable::info info{uint32_t(id)};
        _table.add(allocedStr, info);
        return int(id);
    }


    void SharedKeys::revertToCount(size_t toCount) {
        LOCK(_mutex);
        if (toCount >= _count) {
            throwIf(toCount > count(), SharedKeysStateError, "can't revert to a bigger count");
            return;
        }
        for (auto key = toCount; key < _count; ++key)
            _byKey[key] = nullslice;
        _count = toCount;

        // StringTable doesn't support removing, so rebuild it:
        _table.clear();
        for (size_t key = 0; key < toCount; ++key)
            _table.add(_byKey[key], StringTable::info{uint32_t(key)});
    }



#pragma mark - PERSISTENCE:


    PersistentSharedKeys::PersistentSharedKeys()
    { }


    bool PersistentSharedKeys::refresh() {
        // CBL-87: Race with transactionBegan, possible to enter a transaction and
        // get to here before the transaction reads the new shared keys.  They won't
        // be read here due to _inTransaction being true
        LOCK(_refreshMutex);
        return !_inTransaction && read();
    }


    void PersistentSharedKeys::transactionBegan() {
        // CBL-87: Race with refresh, several lines between here and when new
        // shared keys are actually read leaving a void in between where the shared
        // keys are trying to read but cannot properly be refreshed (via Pusher's
        // sendRevision for example)
        LOCK(_refreshMutex);
        throwIf(_inTransaction, SharedKeysStateError, "already in transaction");
        _inTransaction = true;
        read();     // Catch up with any external changes
    }


    void PersistentSharedKeys::transactionEnded() {
        if (_inTransaction) {
            _committedPersistedCount = _persistedCount;
            _inTransaction = false;
        }
    }


    // Subclass's read() method calls this
    bool PersistentSharedKeys::loadFrom(slice fleeceData) {
        throwIf(changed(), SharedKeysStateError, "can't load when already changed");
        if (!SharedKeys::loadFrom(fleeceData))
            return false;
        _committedPersistedCount = _persistedCount = count();
        return true;
    }


    void PersistentSharedKeys::save() {
        if (changed()) {
            write(stateData());     // subclass hook
            _persistedCount = count();
        }
    }


    void PersistentSharedKeys::revert() {
        revertToCount(_committedPersistedCount);
        _persistedCount = _committedPersistedCount;
    }


    int PersistentSharedKeys::_add(slice str) {
        throwIf(!_inTransaction, SharedKeysStateError, "not in transaction");
        return SharedKeys::_add(str);
    }

} }
