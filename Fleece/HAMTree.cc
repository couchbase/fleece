//
//  HAMTree.cc
//  Fleece
//
//  Copyright © 2018 Couchbase. All rights reserved.
//

#include "HAMTree.hh"
#include <algorithm>
#include <ostream>
#include <string>

using namespace std;

namespace fleece {


#pragma mark - GET

    Val HAMTree::get(Key key) {
        hash_t hash = key.hash();
        LeafNode *leaf = _root.find(hash);
        if (leaf && leaf->_hash == hash && leaf->_key == key)
            return leaf->_val;
        else
            return {};
    }


    HAMTree::LeafNode* HAMTree::InteriorNode::find(hash_t hash) {
        hash_t i = childBitNumber(hash);
        if (!hasChild(i))
            return nullptr;
        Node *child = childForBitNumber(i);
        assert(child);

        if (child->isLeaf())
            return (LeafNode*)child;
        else
            return ((InteriorNode*)child)->find(hash >> kBitSlice);
    }


    void HAMTree::dump(std::ostream &out) {
        out << "HAMTree {\n";
        _root.dump(out);
        out << "}\n";
    }


#pragma mark - INSERT

    void HAMTree::insert(Key key, Val val) {
        _root.insert(key.hash(), 0, key, val);
    }


    HAMTree::InteriorNode* HAMTree::InteriorNode::insert(hash_t hash, unsigned shift, Key key, Val val) {
        assert(shift + kBitSlice < 8*sizeof(hash_t));//TEMP
        hash_t i = childBitNumber(hash, shift);
        if (!hasChild(i)) {
            // No child -- add a leaf:
            return addChildForBitNumber(i, new LeafNode(hash, key, val));
        }
        Node *child = childForBitNumber(i);
        if (child->isLeaf()) {
            // Child is a leaf -- is it the right key?
            LeafNode* leaf = (LeafNode*)child;
            if (leaf->_hash == hash && leaf->_key == key) {
                leaf->_val = val;
                return this;
            }
            // Nope, need to create a new interior node:
            int level = shift / kBitSlice;
            InteriorNode* node = newInteriorNode(2 + (level<1) + (level<3));
            replaceChildForBitNumber(i, node);
            node->addChildForBitNumber(childBitNumber(leaf->_hash, shift+kBitSlice), leaf);
            node->insert(hash, shift+kBitSlice, key, val);
            return this;
        } else {
            // Progress down to interior node...
            auto node = ((InteriorNode*)child)->insert(hash, shift+kBitSlice, key, val);
            if (node != child) {
                childForBitNumber(i) = node;
            }
            return this;
        }
    }


#pragma mark - REMOVE


    bool HAMTree::remove(Key key) {
        return _root.remove(key.hash(), 0, key);
    }


    bool HAMTree::InteriorNode::remove(hash_t hash, unsigned shift, Key key) {
        assert(shift + kBitSlice < 8*sizeof(hash_t));//TEMP
        hash_t i = childBitNumber(hash, shift);
        if (!hasChild(i))
            return false;
        Node *child = childForBitNumber(i);
        if (child->isLeaf()) {
            // Child is a leaf -- is it the right key?
            LeafNode* leaf = (LeafNode*)child;
            if (leaf->_hash == hash && leaf->_key == key) {
                removeChildForBitNumber(i);
                delete leaf;
                return true;
            }
            return false;
        } else {
            // Progress down to interior node...
            auto node = (InteriorNode*)child;
            if (!node->remove(hash, shift+kBitSlice, key))
                return false;
            if (node->_bitmap == 0) {
                removeChildForBitNumber(i);
                delete node;
            }
            return true;
        }
    }


#pragma mark - CHILD ARRAY MANAGEMENT


    uint8_t HAMTree::InteriorNode::childCount() {
        return (uint8_t) std::__pop_count(_bitmap);
    }


    unsigned HAMTree::InteriorNode::itemCount() {
        unsigned count = 0;
        uint8_t n = childCount();
        for (uint8_t i = 0; i < n; ++i) {
            auto child = _children[i];
            if (child->isLeaf())
                count += 1;
            else
                count += ((InteriorNode*)child)->itemCount();
        }
        return count;
    }


    int HAMTree::InteriorNode::childIndexForBitNumber(unsigned bitNumber) {
        return std::__pop_count( _bitmap & ((1u << bitNumber) - 1) );
    }


    HAMTree::Node*& HAMTree::InteriorNode::childForBitNumber(int bitNo) {
        auto i = childIndexForBitNumber(bitNo);
        assert(i < _capacity);
        return _children[i];
    }

    HAMTree::InteriorNode* HAMTree::InteriorNode::addChildForBitNumber(int bitNo, Node *child) {
        InteriorNode* node = (childCount() < _capacity) ? this : grow();
        return node->_addChildForBitNumber(bitNo, child);
    }


    HAMTree::InteriorNode* HAMTree::InteriorNode::_addChildForBitNumber(int bitNo, Node *child) {
        int i = childIndexForBitNumber(bitNo);
        memmove(&_children[i+1], &_children[i], (_capacity - i - 1)*sizeof(Node*));
        _children[i] = child;
        _bitmap |= (1 << bitNo);
        return this;
    }

    void HAMTree::InteriorNode::replaceChildForBitNumber(int bitNo, Node *child) {
        childForBitNumber(bitNo) = child;
    }

    void HAMTree::InteriorNode::removeChildForBitNumber(int bitNo) {
        int i = childIndexForBitNumber(bitNo);
        assert(i < _capacity);
        memmove(&_children[i], &_children[i+1], (_capacity - i - 1)*sizeof(Node*));
        _bitmap &= ~(1 << bitNo);
    }


#pragma mark - HOUSEKEEPING


    void* HAMTree::InteriorNode::operator new(size_t size, uint8_t capacity) {
        return ::operator new(size - (kMaxChildren - capacity)*sizeof(Node*));
    }

    HAMTree::InteriorNode* HAMTree::InteriorNode::newInteriorNode(uint8_t capacity) {
        return new (capacity) InteriorNode(capacity);
    }

    HAMTree::InteriorNode* HAMTree::InteriorNode::grow() {
        assert(_capacity < kMaxChildren);
        InteriorNode* newNode = newInteriorNode(_capacity + 1);
        newNode->_bitmap = _bitmap;
        memcpy(newNode->_children, _children, _capacity*sizeof(Node*));
        delete this;
        return newNode;
    }

    void HAMTree::InteriorNode::freeChildren() {
        uint8_t n = childCount();
        for (uint8_t i = 0; i < n; ++i) {
            auto child = _children[i];
            if (child) {
                if (child->isLeaf())
                    delete (LeafNode*)child;
                else {
                    ((InteriorNode*)child)->freeChildren();
                    delete child;
                }
            }
        }
    }

    void HAMTree::InteriorNode::dump(ostream &out, unsigned indent) {
        uint8_t n = childCount();
        out << string(2*indent, ' ') << "{";
        uint8_t leafCount = n;
        for (uint8_t i = 0; i < n; ++i) {
            auto child = _children[i];
            if (!child->isLeaf()) {
                --leafCount;
                out << "\n";
                ((InteriorNode*)child)->dump(out, indent+1);
            }
        }
        if (leafCount > 0) {
            if (leafCount < n)
                out << "\n" << string(2*indent, ' ') << " ";
            for (uint8_t i = 0; i < n; ++i) {
                auto child = _children[i];
                if (child->isLeaf())
                    ((LeafNode*)child)->dump(out);
            }
        }
        out << " }";
    }


    void HAMTree::LeafNode::dump(std::ostream &out) {
        char str[30];
        sprintf(str, " %08x", _hash);
        out << str;
    }

}
