//
//  SharedKeys.cc
//  Fleece
//
//  Created by Jens Alfke on 10/17/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#include "SharedKeys.hh"
#include "Fleece.hh"
#include "FleeceException.hh"

namespace fleece {
    using namespace std;

    SharedKeys::~SharedKeys()
    { }


    bool SharedKeys::encode(slice str, int &key) const {
        // Is this string already encoded?
        auto &slot = _table.find(str);
        if (_usuallyTrue(slot.first.buf != nullptr)) {
            key = slot.second.offset;
            return true;
        }
        return false;
    }

    bool SharedKeys::encodeAndAdd(slice str, int &key) {
        if (encode(str, key))
            return true;
        // Should this string be encoded?
        if (count() >= _maxCount || str.size > _maxKeyLength || !isEligibleToEncode(str))
            return false;
        // OK, add to table:
        key = add(str);
        return true;
    }


    bool SharedKeys::isEligibleToEncode(slice str) {
        for (size_t i = 0; i < str.size; ++i)
            if (_usuallyFalse(!isalnum(str[i]) && str[i] != '_' && str[i] != '-'))
                return false;
        return true;
    }


    slice SharedKeys::decode(int key) const {
        throwIf(key < 0, InvalidData, "key must be non-negative");
        if (_usuallyFalse(isUnknownKey(key))) {
            // Unrecognized key -- if not in a transaction, try reloading
            const_cast<SharedKeys*>(this)->refresh();
            if (key >= (int)_byKey.size())
                return nullslice;
        }
        return _byKey[key];
    }

    
    SharedKeys::PlatformString SharedKeys::platformStringForKey(int key) const {
        throwIf(key < 0, InvalidData, "key must be non-negative");
        if ((unsigned)key >= _platformStringsByKey.size())
            return nullptr;
        return _platformStringsByKey[key];
    }


    void SharedKeys::setPlatformStringForKey(int key, SharedKeys::PlatformString platformKey) const {
        throwIf(key < 0, InvalidData, "key must be non-negative");
        throwIf((unsigned)key >= _byKey.size(), InvalidData, "key is not yet known");
        auto &strings = const_cast<SharedKeys*>(this)->_platformStringsByKey;
        if ((unsigned)key >= _platformStringsByKey.size())
            strings.resize(key + 1);
        strings[key] = platformKey;
    }


    int SharedKeys::add(slice str) {
        _byKey.emplace_back(str);
        str = _byKey.back();
        auto id = (uint32_t)count();
        StringTable::info info{true, id};
        _table.add(str, info);
        return id;
    }


    void SharedKeys::revertToCount(size_t toCount) {
        if (toCount >= count()) {
            throwIf(toCount > count(), SharedKeysStateError, "can't revert to a bigger count");
            return;
        }
        _byKey.resize(toCount);
        // StringTable doesn't support removing, so rebuild it:
        _table.clear();
        uint32_t key = 0;
        for (auto i = _byKey.begin(); i != _byKey.end(); ++i) {
            StringTable::info info{true, key++};
            _table.add(*i, info);
        }
    }



#pragma mark - PERSISTENCE:


    PersistentSharedKeys::PersistentSharedKeys()
    { }

    
    bool PersistentSharedKeys::refresh() {
        return !_inTransaction && read();
    }


    void PersistentSharedKeys::transactionBegan() {
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
        const Value *v = Value::fromData(fleeceData);
        if (!v)
            return false;
        const Array *strs = v->asArray();
        if (!strs)
            return false;

        Array::iterator i(strs);
        if (i.count() <= count())
            return false;
        i += (unsigned)count();           // Start at the first new string
        for (; i; ++i) {
            slice str = i.value()->asString();
            if (!str)
                return false;
            SharedKeys::add(str);
        }
        _committedPersistedCount = _persistedCount = count();
        return true;
    }


    void PersistentSharedKeys::save() {
        if (!changed())
            return;
        Encoder enc;
        enc.beginArray(count());
        for (auto i = byKey().begin(); i != byKey().end(); ++i)
            enc.writeString(*i);
        enc.endArray();
        write(enc.extractOutput());     // subclass hook
        _persistedCount = count();
    }


    void PersistentSharedKeys::revert() {
        revertToCount(_committedPersistedCount);
        _persistedCount = _committedPersistedCount;
    }


    int PersistentSharedKeys::add(slice str) {
        throwIf(!_inTransaction, SharedKeysStateError, "not in transaction");
        return SharedKeys::add(str);
    }

}
