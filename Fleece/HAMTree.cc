//
//  HAMTree.cc
//  Fleece
//
//  Copyright © 2018 Couchbase. All rights reserved.
//

#include "HAMTree.hh"

namespace fleece {

    Val HAMTree::get(Key key) {
        hash_t hash = key.hash();
        LeafNode *leaf = _root.find(hash);
        if (leaf && leaf->_hash == hash && leaf->_key == key)
            return leaf->_val;
        else
            return {};
    }


    HAMTree::LeafNode* HAMTree::InteriorNode::find(hash_t hash) {
        hash_t i = childIndex(hash);
        if (!hasChild(i))
            return nullptr;
        Node *child = childAtIndex(i);
        assert(child);

        if (child->isLeaf())
            return (LeafNode*)child;
        else
            return ((InteriorNode*)child)->find(hash >> kBitSlice);
    }


    void HAMTree::insert(Key key, Val val) {
        return _root.insert(key.hash(), 0, key, val);
    }


    void HAMTree::InteriorNode::insert(hash_t hash, unsigned shift, Key key, Val val) {
        assert(shift + kBitSlice < 8*sizeof(hash_t));//TEMP
        hash_t i = childIndex(hash, shift);
        if (!hasChild(i)) {
            // No child -- add a leaf:
            setChildAtIndex(i, new LeafNode(hash, key, val));
            _bitmap |= (1 << i);
            return;
        }
        Node *child = childAtIndex(i);
        if (child->isLeaf()) {
            // Child is a leaf -- is it the right key?
            LeafNode* leaf = (LeafNode*)child;
            if (leaf->_hash == hash && leaf->_key == key) {
                leaf->_val = val;
                return;
            }
            // Nope, need to create a new interior node:
            InteriorNode* node = new InteriorNode();
            setChildAtIndex(i, node);
            node->setChildAtIndex(childIndex(leaf->_hash, shift+kBitSlice), leaf);
            node->insert(hash, shift+kBitSlice, key, val);
        } else {
            // Progress down to interior node...
            ((InteriorNode*)child)->insert(hash, shift+kBitSlice, key, val);
        }
    }


    HAMTree::InteriorNode::~InteriorNode() {
        for (uint8_t i = 0; i < _capacity; ++i) {
            auto child = _children[i];
            if (child) {
                if (child->isLeaf())
                    delete (LeafNode*)child;
                else
                    delete (InteriorNode*)child;
            }
        }
    }


}
