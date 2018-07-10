//
// MutableBTree.cc
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

#include "MutableBTree.hh"
#include "MutableDict.hh"
#include "MutableArray.hh"
#include "Encoder.hh"
#include <algorithm>

using namespace std;

namespace fleece {

    static constexpr unsigned kMaxNodeSize = 20;


    // Return value of store operations:
    struct StoreResult {
        Retained<Value>         newChild;       // New child node (may be same as original)
        RetainedConst<Value>    splitKey;       // Key being split on, if there's a split
        Retained<Value>         splitChild;     // New split node, if any
    };


    static StoreResult split(MutableDict *leaf) {
        Retained<MutableDict> newLeaf1 = MutableDict::newDict();
        Retained<MutableDict> newLeaf2 = MutableDict::newDict();
        MutableDict::iterator i(leaf);
        for (unsigned n = 0; n < kMaxNodeSize/2; ++n, ++i)
            newLeaf1->set(i.keyString(), i.value());
        auto newLeafKey = i.keyString();
        for (; i; ++i)
            newLeaf2->set(i.keyString(), i.value());
        return {newLeaf1, NewValue(newLeafKey), newLeaf2};
    }


    static StoreResult storeInLeaf(const Dict* dict, slice key,
                                   MutableBTree::InsertCallback callback)
    {
        // Get the old and new value:
        const Value *oldValue = dict->get(key);
        const Value *value = nullptr;
        if (callback) {
            value = callback(oldValue);
            if (!value)
                value = oldValue;
        }
        if (value == oldValue)
            return {};

        // Make the leaf mutable:
        Retained<MutableDict> leaf = dict->asMutable();
        if (!leaf)
            leaf = MutableDict::newDict(dict);

        if (value) {
            // Insert:
            leaf->set(key, value);
            if (leaf->count() > kMaxNodeSize)
                return split(leaf);
        } else {
            // Delete:
            leaf->remove(key);
            // TODO: The leaf should be merged with a neighbor if it became too small.
        }

        return {leaf};
    }


    static StoreResult split(MutableArray *interior) {
        // Interior node needs to split. Example with kMaxNodeSize = 4:
        // Old node:  [c1 k1 c2 k2 c3 k3 c4 k4 c5]
        // New nodes:  [c1 k1 c2]    [c3 k3 c4 k4 c5]   and k2 is the split key
        // here split=3
        auto count = interior->count();
        constexpr uint32_t split = kMaxNodeSize/2 | 1;
        Retained<MutableArray> interior2 = MutableArray::newArray(count - split - 1);
        for (uint32_t src = split + 1, dst = 0; src < count; ++src, ++dst)
            interior2->set(dst, interior->get(src));
        RetainedConst<Value> splitKey = NewValue(interior->get(split)->asString());
        interior->resize(split);
        assert(interior->count() & 1);
        assert(interior2->count() & 1);
        return {interior, splitKey, interior2};
    }


    static StoreResult storeInInterior(const Array *array, slice key,
                                       MutableBTree::InsertCallback callback)
    {
        uint32_t childIndex = btree::find(array, key);
        const Value *child = array->get(childIndex);
        StoreResult result;
        // TODO: Reimplement without recursion
        if (child->type() == kArray)
            result = storeInInterior((const Array*)child, key, callback);
        else
            result = storeInLeaf((const Dict*)child, key, callback);

        if (result.newChild == nullptr)
            return result;

        // Make the node mutable:
        Retained<MutableArray> interior = array->asMutable();
        if (!interior)
            interior = MutableArray::newArray(array);

        // update child if it was formerly immutable
        if (result.newChild != child)
            interior->set(childIndex, result.newChild);

        if (result.splitChild) {
            // Child node has split, so make room for it:
            interior->insert(childIndex + 1, 2);
            interior->set(childIndex + 1, result.splitKey);
            interior->set(childIndex + 2, result.splitChild);

            if (interior->count() > 2 * kMaxNodeSize)
                return split(interior);
        }

        assert(interior->count() & 1);
        return {interior};
    }



    MutableBTree::MutableBTree()
    :BTree( new MutableDict() )
    { }

    MutableBTree::MutableBTree(const BTree &tree)
    :BTree(tree)
    { }

    bool MutableBTree::insert(slice key, InsertCallback callback) {
        StoreResult result;
        if (_root->type() == kDict)
            result = storeInLeaf((const Dict*)_root.get(), key, callback);
        else
            result = storeInInterior((const Array*)_root.get(), key, callback);

        if (result.newChild == nullptr)
            return false;

        if (result.splitChild) {
            // The root split! Create new root:
            Retained<MutableArray> newRoot = MutableArray::newArray(3);
            newRoot->set(0, result.newChild);
            newRoot->set(1, result.splitKey);
            newRoot->set(2, result.splitChild);
            assert(newRoot->count() & 1);
            _root = newRoot;
        } else {
            // Reassign _root if it went from immutable to mutable:
            if (_root != result.newChild)
                _root = result.newChild;
        }
        return true;
    }

    void MutableBTree::set(slice key, const Value *value) {
        insert(key, [=](const Value *oldValue) {
            return value;
        });
    }

    bool MutableBTree::remove(slice key) {
        return insert(key, nullptr);
    }


    void MutableBTree::writeTo(Encoder &enc) {
        enc.writeValue(_root);
    }


}
