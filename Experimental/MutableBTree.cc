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

// Useful reading: https://www.geeksforgeeks.org/b-tree-set-1-introduction-2/

#include "MutableBTree.hh"
#include "MutableDict.hh"
#include "MutableArray.hh"
#include "Encoder.hh"
#include <algorithm>

using namespace std;

namespace fleece {

    static constexpr unsigned kMaxLeafCount = 20;
    static constexpr unsigned kMinLeafCount = 10;
    static constexpr unsigned kMaxInteriorCount = 21;
    static constexpr unsigned kMinInteriorCount = 10;


    // Return value of store operations:
    struct StoreResult {
        Retained<Value>         curNode;        // New node (may be same as input)
        RetainedConst<Value>    splitKey;       // Key being split on, if there's a split
        Retained<Value>         splitNode;      // New split node, if any
    };


    static inline bool isLeaf(const Value *node) {
        return node->type() == kDict;
    }


    static Retained<MutableArray> makeMutable(const Array *a) {
        Retained<MutableArray> m = a->asMutable();
        return m ? m : MutableArray::newArray(a);
    }


#if 0
    static StoreResult mutateLeaf(const Dict *leaf, MutableArray *parent, uint32_t index) {
        StoreResult result;
        result.curNode = leaf->asMutable();
        if (!result.curNode) {
            result.curNode = (Value*)MutableDict::newDict(leaf);
            if (parent)
                parent->set(index, result.curNode);
        }
        return result;
    }
#endif


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


    static bool mergeLeaf(const Dict *leaf, MutableArray *parent, uint32_t indexInParent) {
        // Look for a neighbor leaf to merge into:
        auto count = leaf->count();
        assert(count < kMinLeafCount);
        const Dict *candidate = nullptr;
        uint32_t candidateIndex = 0;
        if (indexInParent > 1) {
            candidateIndex = indexInParent - 2;
            candidate = parent->get(candidateIndex)->asDict();
            if (candidate && candidate->count() + count >= kMaxLeafCount)
                candidate = nullptr;
        }
        if (!candidate && indexInParent + 2 < parent->count()) {
            candidateIndex = indexInParent + 2;
            candidate = parent->get(candidateIndex)->asDict();
            if (candidate && candidate->count() + count >= kMaxLeafCount)
                candidate = nullptr;
        }
        if (!candidate)
            return false;

        // Now merge 'leaf' into 'candidate':
        Retained<MutableDict> otherLeaf = candidate->asMutable();
        if (!otherLeaf) {
            otherLeaf = MutableDict::newDict(otherLeaf);
            parent->set(candidateIndex, otherLeaf);
        }
        for (Dict::iterator i(leaf); i; ++i)
            otherLeaf->set(i.keyString(), i.value());
        if (candidateIndex < indexInParent)
            parent->remove(indexInParent - 1, 2);
        else
            parent->remove(indexInParent, 2);
        return true;
    }


    static StoreResult storeInLeaf(const Dict* dict, slice key,
                                   MutableBTree::InsertCallback callback)
    {
        // Get the old and new value:
        const Value *oldValue = dict->get(key);
        const Value *value = callback(oldValue);
        if (value == oldValue || value == nullptr)
            return {};

        // Make the leaf mutable:
        Retained<MutableDict> leaf = dict->asMutable();
        if (!leaf)
            leaf = MutableDict::newDict(dict);

        // Store the value:
        leaf->set(key, value);
        if (leaf->count() <= kMaxLeafCount)
            return {leaf};
        else
            return splitLeaf(leaf);
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
        Retained<MutableArray> interior = makeMutable(node);
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


    static bool mergeInterior(const Array *interior, MutableArray *parent, uint32_t indexInParent) {
        abort();
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
        RetainedConst<Dict> leaf = (const Dict*)node;
        StoreResult result = storeInLeaf(leaf, key, callback);
        if (!updateChildInParent(node, parent, indexInParent, result))
            return false;
        return true;
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
        // Walk down to the leaf node, remembering the path:
        vector<pair<const Array*, uint32_t>> path;
        const Value *node = _root;
        while (node->type() == kArray) {
            auto interior = (const Array*)node;
            auto i = btree::find(interior, key);
            path.emplace_back(interior, i);
            node = interior->get(i);
        }

        auto leaf = (const Dict*)node;
        if (!leaf->get(key))
            return false;

        Retained<MutableDict> mleaf = leaf->asMutable();
        if (!mleaf)
            mleaf = MutableDict::newDict(leaf);
        mleaf->remove(key);

        bool childNeedsMerge = (mleaf->count() < kMinLeafCount);
        if (mleaf != leaf || childNeedsMerge) {
            // Walk back up the path, making parents mutable and merging nodes as needed:
            RetainedConst<Value> child(mleaf);
            for (auto i = path.rbegin(); i != path.rend(); ++i) {
                const Array *parent = i->first;
                uint32_t indexInParent = i->second;
                auto mparent = makeMutable(parent);
                mparent->set(indexInParent, child);
                if (childNeedsMerge) {
                    if (isLeaf(child))
                        mergeLeaf(child->asDict(), mparent, indexInParent);
                    else
                        mergeInterior(child->asArray(), mparent, indexInParent);
                }
                childNeedsMerge = (mparent->count() < kMinInteriorCount);
                if (parent == mparent && !childNeedsMerge)
                    return true;        // No more changes to make -- done!
                child = mparent;
            }
            // Finally update the root pointer:
            _root = child;
        }
        return true;
    }


    void MutableBTree::writeTo(Encoder &enc) {
        enc.writeValue(_root);
    }


}
