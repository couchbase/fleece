//
//  MHashTree.cc
//  Fleece
//
//  Copyright © 2018 Couchbase. All rights reserved.
//

#include "MHashTree.hh"
#include "HashTree.hh"
#include "Bitmap.hh"
#include <algorithm>
#include <ostream>
#include <string>

using namespace std;

namespace fleece {


    using hash_t = uint32_t;
    using bitmap_t = Bitmap<uint32_t>;
    static constexpr unsigned kBitShift = 5;                      // must be log2(8*sizeof(bitmap_t))
    static constexpr unsigned kMaxChildren = 1 << kBitShift;
    static_assert(sizeof(bitmap_t) == kMaxChildren / 8, "Wrong constants");


    namespace hashtree {
        using Key = alloc_slice;
        using Val = const Value*;


        class MNode;
        class MTarget;

        // Pointer to any type of node. Mutable nodes are tagged by setting the LSB.
        class NodeRef {
        public:
            NodeRef()                               :_addr(0) { }
            NodeRef(MNode* n)                       :_addr(size_t(n) | 1) {assert(n);}
            NodeRef(const Node* n)                  :_addr(size_t(n)) {assert(n);}
            NodeRef(const Leaf* n)                  :_addr(size_t(n)) {assert(n);}

            operator bool () const                  {return _addr != 0;}

            bool isMutable() const                  {return (_addr & 1) != 0;}

            MNode* asMutable() const {
                return isMutable() ? _asMutable() : nullptr;
            }

            const Node* asImmutable() const {
                return isMutable() ? nullptr : _asImmutable();
            }

            bool isLeaf() const;
            hash_t hash() const;
            bool matches(const MTarget&) const;
            void dump(ostream&, unsigned indent);

        private:
            MNode* _asMutable() const               {return (MNode*)(_addr & ~1);}
            const Node* _asImmutable() const        {return (const Node*)_addr;}

                size_t _addr;
        };


        // Base class of nodes within a MHashTree.
        class MNode {
        public:
            MNode(int8_t capacity)
            :_capacity(capacity)
            { }

            bool isLeaf() const     {return _capacity == 0;}

        protected:
            uint8_t capacity() const {
                assert(_capacity > 0);
                return _capacity;
            }

        private:
            int8_t const _capacity;
        };


        // A node with a key and hash
        class MTarget {
        public:
            MTarget(Key k)
            :MTarget(k.hash(), k)
            { }

            bool matches(hash_t h, Key k) const {
                return _hash == h && _key == k;
            }

            bool matches(const MTarget &target) const {
                return matches(target._hash, target._key);
            }

            hash_t const _hash;
            Key const _key;

        protected:
            MTarget(hash_t h, Key k)
            :_hash(h)
            ,_key(k)
            { }
        };


        // A leaf node that holds a single key and value.
        class MLeafNode : public MNode, public MTarget {
        public:
            MLeafNode(const MTarget &t, const Val &v)
            :MNode(0)
            ,MTarget(t)
            ,_val(v)
            { }

            void dump(std::ostream &out) {
                char str[30];
                sprintf(str, " %08x", _hash);
                out << str;
            }

            Val _val;
        };


        // An interior node holds a small compact hash table mapping to Nodes.
        class MInteriorNode : public MNode {
        public:

            static MInteriorNode* newRoot(const HashTree *imTree) {
                if (imTree)
                    return mutableCopy(imTree->getRoot());
                else
                    return newNode(kMaxChildren);
            }


            void freeChildren() {
                unsigned n = childCount();
                for (unsigned i = 0; i < n; ++i) {
                    auto child = _children[i].asMutable();
                    if (child) {
                        if (child->isLeaf())
                            delete (MLeafNode*)child;
                        else {
                            ((MInteriorNode*)child)->freeChildren();
                            delete child;
                        }
                    }
                }
            }


            unsigned leafCount() const {
                unsigned count = 0;
                unsigned n = childCount();
                for (unsigned i = 0; i < n; ++i) {
                    auto child = _children[i];
                    if (child.isMutable()) {
                        if (child.asMutable()->isLeaf())
                            count += 1;
                        else
                            count += ((MInteriorNode*)child.asMutable())->leafCount();
                    } else {
                        if (child.asImmutable()->isLeaf())
                            count += 1;
                        else
                            count += child.asImmutable()->interior.leafCount();
                    }
                }
                return count;
            }


            NodeRef findNearest(hash_t hash) const {
                unsigned bitNo = childBitNumber(hash);
                if (!hasChild(bitNo))
                    return NodeRef();
                NodeRef child = childForBitNumber(bitNo);
                if (child.isMutable()) {
                    auto mchild = child.asMutable();
                    if (mchild->isLeaf())
                        return child;    // closest match; not guaranteed to have right hash
                    else
                        return ((MInteriorNode*)mchild)->findNearest(hash >> kBitShift);  // recurse...
                } else {
                    auto ichild = child.asImmutable();
                    if (ichild->isLeaf())
                        return child;
                    else
                        return ichild->interior.findNearest(hash);
                }
            }


            MInteriorNode* insert(const MTarget &target, const Val &val, unsigned shift) {
                assert(shift + kBitShift < 8*sizeof(hash_t));//TEMP: Handle hash collisions
                unsigned bitNo = childBitNumber(target._hash, shift);
                if (!hasChild(bitNo)) {
                    // No child -- add a leaf:
                    return addChild(bitNo, new MLeafNode(target, val));
                }
                NodeRef &childRef = childForBitNumber(bitNo);
                if (childRef.isLeaf()) {
                    if (childRef.matches(target._key)) {
                        // Leaf node matches this key; update or copy it:
                        if (childRef.isMutable())
                            ((MLeafNode*)childRef.asMutable())->_val = val;
                        else
                            childRef = new MLeafNode(target, val);
                        return this;
                    } else {
                        // Nope, need to promote the leaf to an interior node & add new key:
                        MInteriorNode *node = promoteLeaf(childRef, shift);
                        node->insert(target, val, shift+kBitShift);
                        return this;
                    }
                } else {
                    // Progress down to interior node...
                    auto child = (MInteriorNode*)childRef.asMutable();
                    if (!child)
                        child = mutableCopy(&childRef.asImmutable()->interior);
                    childRef = child->insert(target, val, shift+kBitShift);
                    return this;
                }
            }


            bool remove(const MTarget &target, unsigned shift) {
                assert(shift + kBitShift < 8*sizeof(hash_t));//TEMP
                unsigned bitNo = childBitNumber(target._hash, shift);
                if (!hasChild(bitNo))
                    return false;
                unsigned childIndex = childIndexForBitNumber(bitNo);
                NodeRef childRef = _children[childIndex];
                if (childRef.isLeaf()) {
                    // Child is a leaf -- is it the right key?
                    if (childRef.matches(target)) {
                        removeChild(bitNo, childIndex);
                        delete (MLeafNode*)childRef.asMutable();
                        return true;
                    } else {
                        return false;
                    }
                } else {
                    // Recurse into child node...
                    auto child = (MInteriorNode*)childRef.asMutable();
                    if (!child)
                        child = mutableCopy(&childRef.asImmutable()->interior);
                    if (!child->remove(target, shift+kBitShift))
                        return false;
                    if (child->_bitmap.empty()) {
                        removeChild(bitNo, childIndex);     // child node is now empty, so remove it
                        delete child;
                    }
                    return true;
                }
            }


            void dump(std::ostream &out, unsigned indent =1) const {
                unsigned n = childCount();
                out << string(2*indent, ' ') << "{";
                unsigned leafCount = n;
                for (unsigned i = 0; i < n; ++i) {
                    auto child = _children[i];
                    if (!child.isLeaf()) {
                        --leafCount;
                        out << "\n";
                        child.dump(out, indent+1);
                    }
                }
                if (leafCount > 0) {
                    if (leafCount < n)
                        out << "\n" << string(2*indent, ' ') << " ";
                    for (unsigned i = 0; i < n; ++i) {
                        auto child = _children[i];
                        if (child.isLeaf())
                            child.dump(out, indent+1);
                    }
                }
                out << " }";
            }


        private:
            static MInteriorNode* newNode(int8_t capacity, MInteriorNode *orig =nullptr) {
                return new (capacity) MInteriorNode(capacity, orig);
            }

            static MInteriorNode* mutableCopy(const Interior *iNode) {
                auto childCount = iNode->childCount();
                auto node = newNode((uint8_t)childCount);
                node->_bitmap = asBitmap(iNode->bitmap());
                for (unsigned i = 0; i < childCount; ++i)
                    node->_children[i] = NodeRef(iNode->childAtIndex(i));
                return node;
            }

            static MInteriorNode* promoteLeaf(NodeRef& childLeaf, unsigned shift) {
                unsigned level = shift / kBitShift;
                MInteriorNode* node = newNode(2 + (level<1) + (level<3));
                unsigned childBitNo = childBitNumber(childLeaf.hash(), shift+kBitShift);
                node->addChild(childBitNo, childLeaf);
                childLeaf = node; // replace immutable node
                return node;
            }

            static void* operator new(size_t size, int8_t capacity) {
                return ::operator new(size + capacity*sizeof(NodeRef));
            }

            MInteriorNode(int8_t cap, MInteriorNode* orig =nullptr)
            :MNode(cap)
            ,_bitmap(orig ? orig->_bitmap : Bitmap<bitmap_t>{})
            {
                if (orig)
                    memcpy(_children, orig->_children, orig->capacity()*sizeof(NodeRef));
                else
                    memset(_children, 0, cap*sizeof(NodeRef));
            }


            MInteriorNode* grow() {
                assert(capacity() < kMaxChildren);
                MInteriorNode* replacement = newNode(capacity() + 1, this);
                delete this;
                return replacement;
            }


            unsigned childCount() const {
                return _bitmap.bitCount();
            }


            static unsigned childBitNumber(hash_t hash, unsigned shift =0)  {
                return (hash >> shift) & (kMaxChildren - 1);
            }


            unsigned childIndexForBitNumber(unsigned bitNo) const {
                return _bitmap.indexOfBit(bitNo);
            }


            bool hasChild(unsigned bitNo) const  {
                return _bitmap.containsBit(bitNo);
            }


            NodeRef& childForBitNumber(unsigned bitNo) {
                auto i = childIndexForBitNumber(bitNo);
                assert(i < capacity());
                return _children[i];
            }


            NodeRef const& childForBitNumber(unsigned bitNo) const {
                return const_cast<MInteriorNode*>(this)->childForBitNumber(bitNo);
            }


            MInteriorNode* addChild(unsigned bitNo, NodeRef child) {
                return addChild(bitNo, childIndexForBitNumber(bitNo), child);
            }

            MInteriorNode* addChild(unsigned bitNo, unsigned childIndex, NodeRef child) {
                MInteriorNode* node = (childCount() < capacity()) ? this : grow();
                return node->_addChild(bitNo, childIndex, child);
            }

            MInteriorNode* _addChild(unsigned bitNo, unsigned childIndex, NodeRef child) {
                assert(child);
                memmove(&_children[childIndex+1], &_children[childIndex],
                        (capacity() - childIndex - 1)*sizeof(NodeRef));
                _children[childIndex] = child;
                _bitmap.addBit(bitNo);
                return this;
            }

            void removeChild(unsigned bitNo, unsigned childIndex) {
                assert(childIndex < capacity());
                memmove(&_children[childIndex], &_children[childIndex+1], (capacity() - childIndex - 1)*sizeof(NodeRef));
                _bitmap.removeBit(bitNo);
            }


            Bitmap<bitmap_t> _bitmap {0};
            NodeRef _children[0];  // Actually dynamic array NodeRef[_capacity]
        };


#pragma mark - NODEREF


        bool NodeRef::isLeaf() const {
            return isMutable() ? _asMutable()->isLeaf() : _asImmutable()->isLeaf();
        }

        hash_t NodeRef::hash() const {
            assert(isLeaf());
            return isMutable() ? ((MLeafNode*)_asMutable())->_hash : _asImmutable()->leaf.hash();
        }

        bool NodeRef::matches(const MTarget &target) const {
            assert(isLeaf());
            return isMutable() ? ((MLeafNode*)_asMutable())->matches(target)
                               : _asImmutable()->leaf.matches(target._key);
        }

        void NodeRef::dump(ostream &out, unsigned indent) {
            if (isMutable())
                isLeaf() ? ((MLeafNode*)_asMutable())->dump(out)
                         : ((MInteriorNode*)_asMutable())->dump(out, indent);
            else
                isLeaf() ? _asImmutable()->leaf.dump(out)
                         : _asImmutable()->interior.dump(out, indent);
        }


    } // end namespace


#pragma mark - MHashTree ITSELF


    using namespace hashtree;

    MHashTree::MHashTree()
    { }

    MHashTree::MHashTree(const HashTree *tree)
    :_imRoot(tree)
    { }

    MHashTree::~MHashTree() {
        if (_root)
            _root->freeChildren();
    }

    unsigned MHashTree::count() const {
        if (_root)
            return _root->leafCount();
        else if (_imRoot)
            return _imRoot->count();
        else
            return 0;
    }

    Val MHashTree::get(const Key &key) const {
        if (_root) {
            hash_t hash = key.hash();
            NodeRef leaf = _root->findNearest(hash);
            if (leaf) {
                if (leaf.isMutable()) {
                    auto mleaf = (MLeafNode*)leaf.asMutable();
                    if (mleaf->matches(hash, key))
                        return mleaf->_val;
                } else {
                    if (leaf.asImmutable()->leaf.matches(key))
                        return leaf.asImmutable()->leaf.value();
                }
            }
        } else if (_imRoot) {
            return _imRoot->get(key);
        }
        return nullptr;
    }

    void MHashTree::insert(const Key &key, Val val) {
        if (!_root)
            _root.reset( MInteriorNode::newRoot(_imRoot) );
        _root->insert(MTarget(key), val, 0);
    }

    bool MHashTree::remove(const Key &key) {
        if (!_root) {
            if (!_imRoot)
                return false;
            _root.reset( MInteriorNode::newRoot(_imRoot) );
        }
        return _root->remove(MTarget(key), 0);
    }

    void MHashTree::dump(std::ostream &out) {
        if (_imRoot && !_root) {
            _imRoot->dump(out);
        } else {
            out << "MHashTree {";
            if (_root) {
                out << "\n";
                _root->dump(out);
            }
            out << "}\n";
        }
    }

}
