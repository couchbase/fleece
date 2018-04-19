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


    using hash_t = size_t;
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



        template <class Key>
        class Target : public Node {
        public:
            Target(Key k)
            :Target(std::hash<Key>()(k), k)
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



        template <class Key, class Val>
        class LeafNode : public Target<Key> {
        public:
            LeafNode(const Target<Key> &t, const Val &v)
            :Target<Key>(t)
            ,_val(v)
            { }

            void dump(std::ostream &out) {
                char str[30];
                sprintf(str, " %08zx", Target<Key>::_hash);
                out << str;
            }

            Val _val;
        };



        template <class Key, class Val>
        class InteriorNode : public Node {
        public:
            using Target = Target<Key>;
            using LeafNode = LeafNode<Key,Val>;


            static InteriorNode* newRoot() {
                return newNode(kMaxChildren);
            }


            void freeChildren() {
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


            unsigned itemCount() const {
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


            LeafNode* findNearest(hash_t hash) const {
                int bitNo = childBitNumber(hash);
                if (!hasChild(bitNo))
                    return nullptr;
                Node *child = childForBitNumber(bitNo);
                if (child->isLeaf())
                    return (LeafNode*)child;    // closest match; not guaranteed to have right hash
                else
                    return ((InteriorNode*)child)->findNearest(hash >> kBitShift);  // recurse...
            }


            InteriorNode* insert(const Target &target, const Val &val, unsigned shift) {
                assert(shift + kBitShift < 8*sizeof(hash_t));//TEMP: Handle hash collisions
                int bitNo = childBitNumber(target._hash, shift);
                if (!hasChild(bitNo)) {
                    // No child -- add a leaf:
                    return addChild(bitNo, new LeafNode(target, val));
                }
                Node* &childRef = childForBitNumber(bitNo);
                if (childRef->isLeaf()) {
                    // Child is a leaf -- is it the right key?
                    LeafNode* leaf = (LeafNode*)childRef;
                    if (leaf->matches(target)) {
                        leaf->_val = val;
                        return this;
                    } else {
                        // Nope, need to create a new interior node:
                        int level = shift / kBitShift;
                        InteriorNode* node = newNode(2 + (level<1) + (level<3));
                        childRef = node;
                        int childBitNo = childBitNumber(leaf->_hash, shift+kBitShift);
                        node->addChild(childBitNo, leaf);
                        node->insert(target, val, shift+kBitShift);
                        return this;
                    }
                } else {
                    // Progress down to interior node...
                    auto node = ((InteriorNode*)childRef)->insert(target, val, shift+kBitShift);
                    if (node != childRef)
                        childRef = node;
                    return this;
                }
            }


            bool remove(const Target &target, unsigned shift) {
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
                    } else {
                        return false;
                    }
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


            void dump(std::ostream &out, unsigned indent =1) {
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


        private:
            static InteriorNode* newNode(uint8_t capacity, InteriorNode *orig =nullptr) {
                return new (capacity) InteriorNode(capacity, orig);
            }

            static void* operator new(size_t size, uint8_t capacity) {
                return ::operator new(size - (kMaxChildren - capacity)*sizeof(Node*));
            }

            InteriorNode(uint8_t capacity, InteriorNode* orig =nullptr)
            :Node(capacity)
            ,_bitmap(orig ? orig->_bitmap : 0)
            {
                if (orig)
                    memcpy(_children, orig->_children, orig->_capacity*sizeof(Node*));
                else
                    memset(_children, 0, _capacity*sizeof(Node*));
            }


            InteriorNode* grow() {
                assert(_capacity < kMaxChildren);
                InteriorNode* replacement = newNode(_capacity + 1, this);
                delete this;
                return replacement;
            }


            uint8_t childCount() const {
                return (uint8_t) std::__pop_count(_bitmap);
            }


            static unsigned childBitNumber(hash_t hash, unsigned shift =0)  {
                return (hash >> shift) & (kMaxChildren - 1);
            }


            int childIndexForBitNumber(unsigned bitNo) const {
                return std::__pop_count( _bitmap & ((bitmap_t(1) << bitNo) - 1) );
            }


            bool hasChild(int i) const  {
                return ((_bitmap & (bitmap_t(1) << i)) != 0);
            }


            Node*& childForBitNumber(int bitNo) {
                auto i = childIndexForBitNumber(bitNo);
                assert(i < _capacity);
                return _children[i];
            }


            Node* const& childForBitNumber(int bitNo) const {
                return const_cast<InteriorNode*>(this)->childForBitNumber(bitNo);
            }


            InteriorNode* addChild(int bitNo, Node *child) {
                return addChild(bitNo, childIndexForBitNumber(bitNo), child);
            }

            InteriorNode* addChild(int bitNo, int childIndex, Node *child) {
                InteriorNode* node = (childCount() < _capacity) ? this : grow();
                return node->_addChild(bitNo, childIndex, child);
            }

            InteriorNode* _addChild(int bitNo, int childIndex, Node *child) {
                assert(child);
                memmove(&_children[childIndex+1], &_children[childIndex],
                        (_capacity - childIndex - 1)*sizeof(Node*));
                _children[childIndex] = child;
                _bitmap |= (bitmap_t(1) << bitNo);
                return this;
            }


            void removeChild(int bitNo, int childIndex) {
                assert(childIndex < _capacity);
                memmove(&_children[childIndex], &_children[childIndex+1], (_capacity - childIndex - 1)*sizeof(Node*));
                _bitmap &= ~(bitmap_t(1) << bitNo);
            }


            bitmap_t _bitmap {0};
            Node* _children[kMaxChildren /*_capacity*/];      // Actual array size is dynamic
        };

    } // end namespace


#pragma mark - HAMTREE ITSELF


    using namespace hamtree;

    template <class Key, class Val>
    HAMTree<Key,Val>::HAMTree()
    { }

    template <class Key, class Val>
    HAMTree<Key,Val>::~HAMTree() {
        if (_root)
            _root->freeChildren();
    }

    template <class Key, class Val>
    unsigned HAMTree<Key,Val>::count() const {
        return _root ? _root->itemCount() : 0;
    }

    template <class Key, class Val>
    Val HAMTree<Key,Val>::get(const Key &key) const {
        if (_root) {
            hash_t hash = std::hash<Key>()(key);
            LeafNode<Key,Val> *leaf = _root->findNearest(hash);
            if (leaf && leaf->matches(hash, key))
                return leaf->_val;
        }
        return {};
    }

    template <class Key, class Val>
    void HAMTree<Key,Val>::insert(const Key &key, Val val) {
        if (!_root)
            _root.reset( InteriorNode<Key,Val>::newRoot() );
        _root->insert(Target<Key>(key), val, 0);
    }

    template <class Key, class Val>
    bool HAMTree<Key,Val>::remove(const Key &key) {
        return _root != nullptr && _root->remove(Target<Key>(key), 0);
    }

    template <class Key, class Val>
    void HAMTree<Key,Val>::dump(std::ostream &out) {
        out << "HAMTree {";
        if (_root) {
            out << "\n";
            _root->dump(out);
        }
        out << "}\n";
    }


    template class HAMTree<alloc_slice, int>;

}
