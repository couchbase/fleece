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


    bool SharedKeys::encode(slice str, int &key) {
        // Is this string already encoded?
        auto &slot = _table.find(str);
        if (_usuallyTrue(slot.first.buf != nullptr)) {
            key = slot.second.offset;
            return true;
        }
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



    slice SharedKeys::decode(int key) {
        throwIf(key < 0, InvalidData, "key must be non-negative");
        if (_usuallyFalse(key >= (int)_byKey.size())) {
            // Unrecognized key -- if not in a transaction, try reloading
            resolveUnknownKey(key);
            if (key >= (int)_byKey.size())
                return nullslice;
        }
        return _byKey[key];
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
            throwIf(toCount > count(), InternalError, "can't revert to a bigger count");
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

    
    void PersistentSharedKeys::update() {
        if (!_inTransaction)
            read();
    }


    void PersistentSharedKeys::transactionBegan() {
        throwIf(_inTransaction, InternalError, "already in transaction");
        _inTransaction = true;
        read();     // Catch up with any external changes
    }

    
    void PersistentSharedKeys::transactionEnded() {
        throwIf(!_inTransaction, InternalError, "not in transaction");
        _committedPersistedCount = _persistedCount;
        _inTransaction = false;
    }


    // Subclass's read() method calls this
    bool PersistentSharedKeys::loadFrom(slice fleeceData) {
        throwIf(changed(), InternalError, "can't load when already changed");
        const Value *v = Value::fromData(fleeceData);
        if (!v)
            return false;
        const Array *strs = v->asArray();
        if (!strs)
            return false;

        Array::iterator i(strs);
        if (i.count() < count())
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
        throwIf(!_inTransaction, InternalError, "not in transaction");
        return SharedKeys::add(str);
    }


    void PersistentSharedKeys::resolveUnknownKey(int key) {
        // Presumably this is a new key that was added to the persistent mapping, so re-read it:
        if (!_inTransaction)
            read();
//        if (key >= (int)_byKey.size()) {
//            Warn("SharedKeys: Unrecognized key %d not in persistent storage (max=%ld)",
//                 key, (long)_byKey.size());
//        }
    }

}
