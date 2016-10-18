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

    static const size_t kMaxCount = 2048;       // Max number of keys to store
    static const size_t kMaxKeySize = 16;       // Max length of string to store


    void SharedKeys::update() {
        if (!_inTransaction)
            read();
    }


    void SharedKeys::transactionBegan() {
        throwIf(_inTransaction, InternalError, "already in transaction");
        _inTransaction = true;
        read();     // Catch up with any external changes
    }

    
    void SharedKeys::transactionEnded() {
        throwIf(!_inTransaction, InternalError, "not in transaction");
        _committedPersistedCount = _persistedCount;
        _inTransaction = false;
    }


    // Subclass's read() method calls this
    bool SharedKeys::loadFrom(slice fleeceData) {
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
            add(str);
        }
        _committedPersistedCount = _persistedCount = count();
        return true;
    }


    void SharedKeys::save() {
        if (!changed())
            return;
        Encoder enc;
        enc.beginArray(_table.count());
        for (auto i = _byKey.begin(); i != _byKey.end(); ++i)
            enc.writeString(*i);
        enc.endArray();
        write(enc.extractOutput());     // subclass hook
        _persistedCount = count();
    }


    void SharedKeys::revert() {
        if (count() <= _committedPersistedCount)
            return;
        _persistedCount = _committedPersistedCount;
        _byKey.resize(_committedPersistedCount);
        // StringTable doesn't support removing, so rebuild it:
        _table.clear();
        uint32_t key = 0;
        for (auto i = _byKey.begin(); i != _byKey.end(); ++i) {
            StringTable::info info{true, key++};
            _table.add(*i, info);
        }
    }


    bool SharedKeys::encode(slice str, int &key) {
        throwIf(!_inTransaction, InternalError, "not in transaction");
        // Is this string already encoded?
        auto slot = _table.find(str);
        if (slot) {
            key = slot->second.offset;
            return true;
        }
        // Should this string be encoded?
        if (count() >= kMaxCount)
            return false;
        if (str.size > kMaxKeySize)
            return false;
        for (size_t i = 0; i < str.size; ++i)
            if (!isalnum(str[i]) && str[i] != '_' && str[i] != '-')
                return false;
        // OK, add to table:
        key = add(str);
        return true;
    }


    slice SharedKeys::decode(int key) {
        throwIf(key < 0, InvalidData, "key must be non-negative");
        if (key >= (int)_byKey.size()) {
            // Unrecognized key -- if not in a transaction, try reloading
            if (!_inTransaction)
                read();
            if (key >= (int)_byKey.size()) {
                FleeceException::_throw(InvalidData, "Unrecognized key not in persistent storage");
//                Warn("SharedKeys: Unrecognized key %d not in persistent storage (max=%ld)",
//                     key, (long)_byKey.size());
                return nullslice;
            }
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

}
