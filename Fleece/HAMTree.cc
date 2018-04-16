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
        if (leaf && leaf->matches(hash, key))
            return leaf->_val;
        else
            return {};
    }


    HAMTree::LeafNode* HAMTree::InteriorNode::find(hash_t hash) const {
        int bitNo = childBitNumber(hash);
        if (!hasChild(bitNo))
            return nullptr;
        Node *child = childForBitNumber(bitNo);
        if (child->isLeaf())
            return (LeafNode*)child;
        else
            return ((InteriorNode*)child)->find(hash >> kBitShift);
    }


    void HAMTree::dump(std::ostream &out) {
        out << "HAMTree {\n";
        _root.dump(out);
        out << "}\n";
    }


#pragma mark - INSERT

    void HAMTree::insert(Key key, Val val) {
        _root.insert(LeafNode(key, val), 0);
    }


    HAMTree::InteriorNode*
    HAMTree::InteriorNode::insert(const LeafNode &target, unsigned shift) {
        assert(shift + kBitShift < 8*sizeof(hash_t));//TEMP: Handle hash collisions
        int bitNo = childBitNumber(target._hash, shift);
        if (!hasChild(bitNo)) {
            // No child -- add a leaf:
            return addChild(bitNo, new LeafNode(target));
        }
        Node* &childRef = childForBitNumber(bitNo);
        if (childRef->isLeaf()) {
            // Child is a leaf -- is it the right key?
            LeafNode* leaf = (LeafNode*)childRef;
            if (leaf->matches(target)) {
                leaf->_val = target._val;
                return this;
            } else {
                // Nope, need to create a new interior node:
                int level = shift / kBitShift;
                InteriorNode* node = newInteriorNode(2 + (level<1) + (level<3));
                childRef = node;
                int childBitNo = childBitNumber(leaf->_hash, shift+kBitShift);
                node->addChild(childBitNo, leaf);
                node->insert(target, shift+kBitShift);
                return this;
            }
        } else {
            // Progress down to interior node...
            auto node = ((InteriorNode*)childRef)->insert(target, shift+kBitShift);
            if (node != childRef)
                childRef = node;
            return this;
        }
    }


#pragma mark - REMOVE


    bool HAMTree::remove(Key key) {
        return _root.remove(Target(key), 0);
    }


    bool HAMTree::InteriorNode::remove(const Target &target, unsigned shift) {
        assert(shift + kBitShift < 8*sizeof(hash_t));//TEMP
        int bitNo = childBitNumber(target._hash, shift);
        if (!hasChild(bitNo))
            return false;
        int childIndex = childIndexForBitNumber(bitNo);
        Node *child = _children[childIndex];
        if (child->isLeaf()) {
            // Child is a leaf -- is it the right key?
            LeafNode* leaf = (LeafNode*)child;
            if (leaf->matches(target)) {
                removeChild(bitNo, childIndex);
                delete leaf;
                return true;
            }
            return false;
        } else {
            // Recurse into child node...
            auto node = (InteriorNode*)child;
            if (!node->remove(target, shift+kBitShift))
                return false;
            if (node->_bitmap == 0) {
                removeChild(bitNo, childIndex);     // child node is now empty, so remove it
                delete node;
            }
            return true;
        }
    }


#pragma mark - CHILD ARRAY MANAGEMENT


    uint8_t HAMTree::InteriorNode::childCount() const {
        return (uint8_t) std::__pop_count(_bitmap);
    }


    unsigned HAMTree::InteriorNode::itemCount() const {
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


    int HAMTree::InteriorNode::childIndexForBitNumber(unsigned bitNo) const {
        return std::__pop_count( _bitmap & ((bitmap_t(1) << bitNo) - 1) );
    }


    HAMTree::Node*&
    HAMTree::InteriorNode::childForBitNumber(int bitNo) {
        auto i = childIndexForBitNumber(bitNo);
        assert(i < _capacity);
        return _children[i];
    }

    HAMTree::InteriorNode*
    HAMTree::InteriorNode::addChild(int bitNo, int childIndex, Node *child) {
        InteriorNode* node = (childCount() < _capacity) ? this : grow();
        return node->_addChildForBitNumber(bitNo, childIndex, child);
    }


    HAMTree::InteriorNode*
    HAMTree::InteriorNode::_addChildForBitNumber(int bitNo, int i, Node *child) {
        memmove(&_children[i+1], &_children[i], (_capacity - i - 1)*sizeof(Node*));
        _children[i] = child;
        _bitmap |= (bitmap_t(1) << bitNo);
        return this;
    }

    void HAMTree::InteriorNode::removeChild(int bitNo, int i) {
        assert(i < _capacity);
        memmove(&_children[i], &_children[i+1], (_capacity - i - 1)*sizeof(Node*));
        _bitmap &= ~(bitmap_t(1) << bitNo);
    }


#pragma mark - HOUSEKEEPING


    void* HAMTree::InteriorNode::operator new(size_t size, uint8_t capacity) {
        return ::operator new(size - (kMaxChildren - capacity)*sizeof(Node*));
    }

    HAMTree::InteriorNode*
    HAMTree::InteriorNode::newInteriorNode(uint8_t capacity, InteriorNode *orig) {
        return new (capacity) InteriorNode(capacity, orig);
    }

    HAMTree::InteriorNode*
    HAMTree::InteriorNode::grow() {
        assert(_capacity < kMaxChildren);
        InteriorNode* newNode = newInteriorNode(_capacity + 1, this);
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
