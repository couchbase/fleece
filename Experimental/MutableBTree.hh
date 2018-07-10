//
// MutableBTree.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "BTree.hh"
#include "RefCounted.hh"
#include <functional>
#include <memory>

namespace fleece {
    class MutableArray;
    class MutableDict;
    class Encoder;


    class MutableBTree : public BTree {
    public:
        MutableBTree();
        MutableBTree(const BTree&);
        ~MutableBTree() =default;

        MutableArray* getMutableArray(slice key);
        MutableDict* getMutableDict(slice key);

        bool isChanged() const                  {return _root->isMutable();}

        using InsertCallback = std::function<const Value*(const Value*)>;

        void set(slice key, const Value*);
        bool insert(slice key, InsertCallback);
        bool remove(slice key);

        void writeTo(Encoder&);

        using iterator = BTree::iterator;

    private:
        Value* getMutable(slice key, internal::tags ifType);

        friend class BTree::iterator;
    };
}
