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


    using hash_t = uint32_t;
    using bitmap_t = uint64_t;
    static constexpr int kBitShift = 6;                      // must be log2(8*sizeof(bitmap_t))
    static constexpr int kMaxChildren = 1 << kBitShift;
    static_assert(sizeof(bitmap_t) == kMaxChildren / 8, "Wrong constants");


    namespace hamtree {

        class Node {
        public:
            Node(uint8_t capacity)
            :_capacity(capacity)
            { }

            bool isLeaf() const {return _capacity == 0;}

        protected:
            uint8_t const _capacity;
        };


        class Target : public Node {
        public:
            Target(Key k)
            :Target(k.hash(), k)
            { }

            bool matches(hash_t h, Key k) const {
                return _hash == h && _key == k;
            }

            bool matches(const Target &target) {
                return matches(target._hash, target._key);
            }

            hash_t const _hash;
            Key const _key;

        protected:
            Target(hash_t h, Key k)
            :Node(0)
            ,_hash(h)
            ,_key(k)
            { }
        };


        class LeafNode : public Target {
        public:
            LeafNode(hash_t h, Key k, Val v)
            :Target(h, k)
            ,_val(v)
            { }

            LeafNode(Key k, Val v)
            :LeafNode(k.hash(), k, v)
            { }

            void dump(std::ostream&);

            Val _val;
        };


        class InteriorNode : public Node {
        public:
            static InteriorNode* newInteriorNode(uint8_t capacity, InteriorNode *orig =nullptr);
            void freeChildren();

            uint8_t childCount() const;
            unsigned itemCount() const;
            LeafNode* find(hash_t) const;
            InteriorNode* insert(const LeafNode &target, unsigned shift);
            bool remove(const Target &target, unsigned shift);

            void dump(std::ostream &out, unsigned indent =1);

        private:
            static void* operator new(size_t, uint8_t capacity);

            InteriorNode(uint8_t capacity, InteriorNode* orig =nullptr)
            :Node(capacity)
            ,_bitmap(orig ? orig->_bitmap : 0)
            {
                if (orig)
                    memcpy(_children, orig->_children, orig->_capacity*sizeof(Node*));
                else
                    memset(_children, 0, _capacity*sizeof(Node*));
            }

            static unsigned childBitNumber(hash_t hash, unsigned shift =0)  {
                return (hash >> shift) & (kMaxChildren - 1);
            }

            int childIndexForBitNumber(unsigned bitNumber) const;

            bool hasChild(int i) const  {
                return ((_bitmap & (bitmap_t(1) << i)) != 0);
            }

            Node*& childForBitNumber(int bitNo);
            Node* const& childForBitNumber(int bitNo) const {
                return const_cast<InteriorNode*>(this)->childForBitNumber(bitNo);
            }
            InteriorNode* addChild(int bitNo, int childIndex, Node *child);
            InteriorNode* addChild(int bitNo, Node *child) {
                return addChild(bitNo, childIndexForBitNumber(bitNo), child);
            }
            InteriorNode* _addChildForBitNumber(int bitNo, int childIndex, Node *child);
            void removeChild(int bitNo, int childIndex);

            InteriorNode* grow();

            bitmap_t _bitmap {0};
            Node* _children[kMaxChildren];
        };

    } // end namespace

    using namespace hamtree;


#pragma mark - HAMTREE


    HAMTree::HAMTree()
    { }

    HAMTree::~HAMTree() {
        if (_root)
            _root->freeChildren();
    }


    unsigned HAMTree::count() const {
        return _root ? _root->itemCount() : 0;
    }


    Val HAMTree::get(Key key) const {
        if (_root) {
            hash_t hash = key.hash();
            LeafNode *leaf = _root->find(hash);
            if (leaf && leaf->matches(hash, key))
                return leaf->_val;
        }
        return {};
    }


    LeafNode* InteriorNode::find(hash_t hash) const {
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
        if (_root)
            _root->dump(out);
        out << "}\n";
    }


#pragma mark - INSERT

    void HAMTree::insert(Key key, Val val) {
        if (!_root)
            _root.reset( InteriorNode::newInteriorNode(kMaxChildren) );
        _root->insert(LeafNode(key, val), 0);
    }


    InteriorNode*
    InteriorNode::insert(const LeafNode &target, unsigned shift) {
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
        return _root != nullptr && _root->remove(Target(key), 0);
    }


    bool InteriorNode::remove(const Target &target, unsigned shift) {
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


    uint8_t InteriorNode::childCount() const {
        return (uint8_t) std::__pop_count(_bitmap);
    }


    unsigned InteriorNode::itemCount() const {
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


    int InteriorNode::childIndexForBitNumber(unsigned bitNo) const {
        return std::__pop_count( _bitmap & ((bitmap_t(1) << bitNo) - 1) );
    }


    Node*& InteriorNode::childForBitNumber(int bitNo) {
        auto i = childIndexForBitNumber(bitNo);
        assert(i < _capacity);
        return _children[i];
    }

    InteriorNode* InteriorNode::addChild(int bitNo, int childIndex, Node *child) {
        InteriorNode* node = (childCount() < _capacity) ? this : grow();
        return node->_addChildForBitNumber(bitNo, childIndex, child);
    }


    InteriorNode* InteriorNode::_addChildForBitNumber(int bitNo, int i, Node *child) {
        memmove(&_children[i+1], &_children[i], (_capacity - i - 1)*sizeof(Node*));
        _children[i] = child;
        _bitmap |= (bitmap_t(1) << bitNo);
        return this;
    }

    void InteriorNode::removeChild(int bitNo, int i) {
        assert(i < _capacity);
        memmove(&_children[i], &_children[i+1], (_capacity - i - 1)*sizeof(Node*));
        _bitmap &= ~(bitmap_t(1) << bitNo);
    }


#pragma mark - HOUSEKEEPING


    void* InteriorNode::operator new(size_t size, uint8_t capacity) {
        return ::operator new(size - (kMaxChildren - capacity)*sizeof(Node*));
    }

    InteriorNode*
    InteriorNode::newInteriorNode(uint8_t capacity, InteriorNode *orig) {
        return new (capacity) InteriorNode(capacity, orig);
    }

    InteriorNode*
    InteriorNode::grow() {
        assert(_capacity < kMaxChildren);
        InteriorNode* newNode = newInteriorNode(_capacity + 1, this);
        delete this;
        return newNode;
    }

    void InteriorNode::freeChildren() {
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

    void InteriorNode::dump(ostream &out, unsigned indent) {
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


    void LeafNode::dump(std::ostream &out) {
        char str[30];
        sprintf(str, " %08x", _hash);
        out << str;
    }

}
