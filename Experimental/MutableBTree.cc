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

    static constexpr unsigned kMaxLeafCount = 20;
    static constexpr unsigned kMaxInteriorCount = 21;


    // Return value of store operations:
    struct StoreResult {
        Retained<Value>         curNode;        // New node (may be same as input)
        RetainedConst<Value>    splitKey;       // Key being split on, if there's a split
        Retained<Value>         splitNode;      // New split node, if any
    };


    static inline bool isLeaf(const Value *node) {
        return node->type() == kDict;
    }


    static StoreResult splitLeaf(MutableDict *leaf) {
        assert(leaf->count() >= kMaxLeafCount);
        Retained<MutableDict> newLeaf1 = MutableDict::newDict();
        Retained<MutableDict> newLeaf2 = MutableDict::newDict();
        MutableDict::iterator i(leaf);
        for (unsigned n = 0; n < kMaxLeafCount/2; ++n, ++i)
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
            leaf->set(key, value);
            if (leaf->count() > kMaxLeafCount)
                return splitLeaf(leaf);
        } else {
            leaf->remove(key); // TODO: The leaf should be merged with a neighbor if it became too small.
        }
        assert(leaf->count() <= kMaxLeafCount);
        return {leaf};
    }


    static StoreResult splitInterior(MutableArray *interior) {
        assert(interior->count() == kMaxInteriorCount);
        // Interior node needs to split. Example with kMaxInteriorCount = 9 (and split=3):
        // Old node:  [c1 k1 c2 k2 c3 k3 c4 k4 c5]
        // New nodes:  [c1 k1 c2]    [c3 k3 c4 k4 c5]   and k2 is the split key
        constexpr uint32_t split = kMaxInteriorCount/2 | 1;
        Retained<MutableArray> interior2 = MutableArray::newArray(kMaxInteriorCount - split - 1);
        for (uint32_t src = split + 1, dst = 0; src < kMaxInteriorCount; ++src, ++dst)
            interior2->set(dst, interior->get(src));
        RetainedConst<Value> splitKey = NewValue(interior->get(split)->asString());
        interior->resize(split);
        assert(interior->count() & 1);
        assert(interior2->count() & 1);
        return {interior, splitKey, interior2};
    }


    static StoreResult maybeSplitInterior(const Array *node) {
        Retained<MutableArray> interior = node->asMutable();
        if (!interior)
            interior = MutableArray::newArray(node);
        if (interior->count() < kMaxInteriorCount - 1)
            return {interior};
        else
            return splitInterior(interior);
    }


    static void insertSplitChild(MutableArray *interior,
                                 uint32_t childIndex,
                                 const StoreResult &result)
    {
        interior->insert(childIndex + 1, 2);
        interior->set(childIndex + 0, result.curNode);
        interior->set(childIndex + 1, result.splitKey);
        interior->set(childIndex + 2, result.splitNode);
        assert(interior->count() <= kMaxInteriorCount);
    }



    MutableBTree::MutableBTree()
    :BTree( new MutableDict() )
    { }

    MutableBTree::MutableBTree(const BTree &tree)
    :BTree(tree)
    { }

    bool MutableBTree::insert(slice key, InsertCallback callback) {
        const Value *node = _root;
        MutableArray *parent = nullptr;
        uint32_t indexInParent = 0;

        // Traverse interior nodes, stopping when we reach a leaf:
        while (!isLeaf(node)) {
            // Find the next child to traverse to:
            auto interior = (const Array*)node;
            uint32_t childIndex = btree::find(interior, key);
            const Value *child = interior->get(childIndex);

            // Split the current interior node if it's full:
            StoreResult result = maybeSplitInterior(interior);
            if (!updateChildInParent(node, parent, indexInParent, result))
                return false;

            // Go to the child:
            parent = (MutableArray*)result.curNode.get();
            if (result.splitKey && key >= result.splitKey->asString()) {
                childIndex -= parent->count() + 1;
                parent = (MutableArray*)result.splitNode.get();
            }
            indexInParent = childIndex;
            node = child;
            assert(parent->get(indexInParent) == node);
        }

        // Insert into the leaf (which may split):
        StoreResult result = storeInLeaf((const Dict*)node, key, callback);
        return updateChildInParent(node, parent, indexInParent, result);
    }

    bool MutableBTree::updateChildInParent(const Value *node,
                                           MutableArray *parent,
                                           uint32_t indexInParent,
                                           const StoreResult &result)
    {
        if (result.splitNode) {
            if (parent)
                insertSplitChild(parent, indexInParent, result);
            else
                splitRoot(result);
        } else if (result.curNode) {
            if (result.curNode != node) {
                if (parent)
                    parent->set(indexInParent, result.curNode);
                else
                    _root = result.curNode;
            }
        } else {
            return false;
        }
        return true;
    }

    void MutableBTree::splitRoot(const StoreResult &result) {
        Retained<MutableArray> newRoot = MutableArray::newArray(3);
        newRoot->set(0, result.curNode);
        newRoot->set(1, result.splitKey);
        newRoot->set(2, result.splitNode);
        _root = newRoot;
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
