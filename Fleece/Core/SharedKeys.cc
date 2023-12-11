//
// SharedKeys.cc
//
// Copyright 2016-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "SharedKeys.hh"
#include "FleeceImpl.hh"
#include "FleeceException.hh"


#define LOCK(MUTEX)     lock_guard<mutex> _lock(MUTEX)


namespace fleece { namespace impl {
    using namespace std;


    SharedKeys::SharedKeys()
    :_table(2047)
    { }


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


    size_t SharedKeys::count() const {
        LOCK(_mutex);
        return _count;
    }


    bool SharedKeys::loadFrom(slice stateData) {
        return loadFrom(Value::fromData(stateData));
    }


    bool SharedKeys::loadFrom(const Value *state) {
        if (!state)
            return false;
        Array::iterator i(state->asArray());
        LOCK(_mutex);
        if (i.count() <= _count)
            return false;

        i += _count;           // Start at the first _new_ string
        for (; i; ++i) {
            slice str = i.value()->asString();
            if (!str)
                return false;
            int key;
            if (!SharedKeys::_add(str, key))
                return false;
        }
        return true;
    }


    void SharedKeys::writeState(Encoder &enc) const {
        auto count = _count;
        enc.beginArray(count);
        for (size_t key = 0; key < count; ++key)
            enc.writeString(_byKey[key]);
        enc.endArray();
    }


    alloc_slice SharedKeys::stateData() const {
        Encoder enc;
        writeState(enc);
        return enc.finish();
    }


    bool SharedKeys::encode(slice str, int &key) const {
        // Is this string already encoded?
        auto entry = _table.find(str);
        if (_usuallyTrue(entry.key != nullslice)) {
            key = entry.value;
            return true;
        }
        return false;
    }


    bool SharedKeys::encodeAndAdd(slice str, int &key) {
        if (encode(str, key))
            return true;
        // Should this string be encoded?
        if (str.size > _maxKeyLength || !isEligibleToEncode(str))
            return false;
        LOCK(_mutex);
        if (_count >= kMaxCount)
            return false;
        throwIf(!_inTransaction, SharedKeysStateError, "not in transaction");
        // OK, add to table:
        return _add(str, key);
    }


    bool SharedKeys::_add(slice str, int &key) {
        auto value = uint16_t(_count);
        auto entry = _table.insert(str, value);
        if (!entry.key)
            return false; // failed

        if (entry.value == value) {
            // new key:
            _byKey[value] = entry.key;
            ++_count;
        }
        key = entry.value;
        return true;
    }


    __hot bool SharedKeys::isEligibleToEncode(slice str) const {
        for (size_t i = 0; i < str.size; ++i)
            if (_usuallyFalse(!isalnum(str[i]) && str[i] != '_' && str[i] != '-'))
                return false;
        return true;
    }


    bool SharedKeys::isUnknownKey(int key) const {
        LOCK(_mutex);
        return _isUnknownKey(key);
    }


    /** Decodes an integer back to a string. */
    slice SharedKeys::decode(int key) const {
        throwIf(key < 0, InvalidData, "key must be non-negative");
        if (_usuallyFalse(key >= kMaxCount))
            return nullslice;
        slice str = _byKey[key];
        if (_usuallyFalse(!str))
            return decodeUnknown(key);
        return str;
    }


    slice SharedKeys::decodeUnknown(int key) const {
        // Unrecognized key -- if not in a transaction, try reloading
        const_cast<SharedKeys*>(this)->refresh();

        // Retry after refreshing:
        LOCK(_mutex);
        return _byKey[key];
    }


    vector<slice> SharedKeys::byKey() const {
        LOCK(_mutex);
        return vector<slice>(&_byKey[0], &_byKey[_count]);
    }


    SharedKeys::PlatformString SharedKeys::platformStringForKey(int key) const {
        throwIf(key < 0, InvalidData, "key must be non-negative");
        LOCK(_mutex);
        if ((unsigned)key >= _platformStringsByKey.size())
            return nullptr;
        return _platformStringsByKey[key];
    }


    void SharedKeys::setPlatformStringForKey(int key, SharedKeys::PlatformString platformKey) const {
        LOCK(_mutex);
        throwIf(key < 0, InvalidData, "key must be non-negative");
        throwIf((unsigned)key >= _count, InvalidData, "key is not yet known");
        if ((unsigned)key >= _platformStringsByKey.size())
            _platformStringsByKey.resize(key + 1);
#ifdef __APPLE__
        _platformStringsByKey[key] = CFStringCreateCopy(kCFAllocatorDefault, platformKey);
#else
        _platformStringsByKey[key] = platformKey;
#endif
    }


    void SharedKeys::revertToCount(size_t toCount) {
        LOCK(_mutex);
        if (toCount >= _count) {
            throwIf(toCount > _count, SharedKeysStateError, "can't revert to a bigger count");
            return;
        }

        // (Iterating backwards helps the ConcurrentArena free up key space.)
        for (int key = _count - 1; key >= int(toCount); --key) {
            _table.remove(_byKey[key]);
            _byKey[key] = nullslice;
        }
        _count = unsigned(toCount);
    }



#pragma mark - PERSISTENCE:


    PersistentSharedKeys::PersistentSharedKeys() {
        _inTransaction = false;
    }


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
        disableCaching();
        read();     // Catch up with any external changes
    }


    void PersistentSharedKeys::transactionEnded() {
        if (_inTransaction) {
            _committedPersistedCount = _persistedCount;
            _inTransaction = false;
            enableCaching();
        }
    }


    // Subclass's read() method calls this
    bool PersistentSharedKeys::loadFrom(const Value *state) {
        throwIf(changed(), SharedKeysStateError, "can't load when already changed");
        if (!SharedKeys::loadFrom(state))
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

} }
