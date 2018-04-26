//
//  MHashTree.cc
//  Fleece
//
//  Copyright © 2018 Couchbase. All rights reserved.
//

#include "MHashTree.hh"
#include "HashTree.hh"
#include "HashTree+Internal.hh"
#include "Bitmap.hh"
#include "Encoder.hh"
#include <algorithm>
#include <ostream>
#include <string>

using namespace std;

namespace fleece {

    namespace hashtree {
        class MNode;


        using offset_t = int32_t;


        // Just a key and its hash
        struct Target {
            explicit Target(slice k) :key(k), hash(k.hash()) { }

            bool operator== (const Target &b) const {
                return hash == b.hash && key == b.key;
            }

            slice const key;
            hash_t const hash;
        };


        // Holds a pointer to any type of node. Mutable nodes are tagged by setting the LSB.
        class NodeRef {
        public:
            NodeRef()                               :_addr(0) { }
            NodeRef(MNode* n)                       :_addr(size_t(n) | 1) {assert(n);}
            NodeRef(const Node* n)                  :_addr(size_t(n)) {}
            NodeRef(const Leaf* n)                  :_addr(size_t(n)) {}

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
            bool matches(Target) const;
            Node writeTo(Encoder &enc);
            void dump(ostream&, unsigned indent) const;

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

            static void encodeOffset(offset_t &o, size_t curPos) {
                assert((ssize_t)curPos > o);
                o = _encLittle32(offset_t(curPos - o));
            }

        protected:
            uint8_t capacity() const {
                assert(_capacity > 0);
                return _capacity;
            }

        private:
            int8_t const _capacity;
        };


        // A leaf node that holds a single key and value.
        class MLeafNode : public MNode {
        public:
            MLeafNode(const Target &t, const Value *v)
            :MNode(0)
            ,_hash(t.hash)
            ,_key(t.key)
            ,_value(v)
            { }

            bool matches(Target target) const {
                return _hash == target.hash && _key == target.key;
            }

            Leaf writeTo(Encoder &enc) {
                enc.writeString(_key);
                auto keyPos = enc.finishItem();
                enc.writeValue(_value);
                auto valPos = enc.finishItem();
                return Leaf(offset_t(keyPos), offset_t(valPos));
            }

            void dump(std::ostream &out, unsigned indent) {
                char hashStr[30];
                sprintf(hashStr, "{%08x ", _hash);
                out << string(2*indent, ' ') << hashStr << '"';
                out.write((char*)_key.buf, _key.size);
                out << "\"=" << _value->toJSONString() << "}";
            }

            alloc_slice const _key;
            hash_t const _hash;
            const Value* _value;
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


            void deleteTree() {
                unsigned n = childCount();
                for (unsigned i = 0; i < n; ++i) {
                    auto child = _children[i].asMutable();
                    if (child) {
                        if (child->isLeaf())
                            delete (MLeafNode*)child;
                        else
                            ((MInteriorNode*)child)->deleteTree();
                    }
                }
                delete this;
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
                if (child.isLeaf()) {
                    return child;
                } else if (child.isMutable()) {
                    auto mchild = child.asMutable();
                    return ((MInteriorNode*)mchild)->findNearest(hash >> kBitShift);  // recurse...
                } else {
                    auto ichild = child.asImmutable();
                    return ichild->interior.findNearest(hash >> kBitShift);
                }
            }


            MInteriorNode* insert(Target target, const Value *val, unsigned shift) {
                assert(shift + kBitShift < 8*sizeof(hash_t));//FIX: //TODO: Handle hash collisions
                unsigned bitNo = childBitNumber(target.hash, shift);
                if (!hasChild(bitNo)) {
                    // No child -- add a leaf:
                    return addChild(bitNo, new MLeafNode(target, val));
                }
                NodeRef &childRef = childForBitNumber(bitNo);
                if (childRef.isLeaf()) {
                    if (childRef.matches(target)) {
                        // Leaf node matches this key; update or copy it:
                        if (childRef.isMutable())
                            ((MLeafNode*)childRef.asMutable())->_value = val;
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


            bool remove(Target target, unsigned shift) {
                assert(shift + kBitShift < 8*sizeof(hash_t));//TEMP
                unsigned bitNo = childBitNumber(target.hash, shift);
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
                    if (child) {
                        if (!child->remove(target, shift+kBitShift))
                            return false;
                    } else {
                        child = mutableCopy(&childRef.asImmutable()->interior);
                        if (!child->remove(target, shift+kBitShift)) {
                            delete child;
                            return false;
                        }
                        _children[childIndex] = child;
                    }
                    if (child->_bitmap.empty()) {
                        removeChild(bitNo, childIndex);     // child node is now empty, so remove it
                        delete child;
                    }
                    return true;
                }
            }


            static offset_t encodeImmutableOffset(const Node *inode, offset_t off, const Encoder &enc) {
                ssize_t o = (((char*)inode - (char*)enc.base().buf) - off) - enc.base().size;
                assert(o < 0 && o > INT32_MIN);
                return offset_t(o);
            }


            Interior writeTo(Encoder &enc) {
                unsigned n = childCount();
                Node nodes[n];

                // Let each (mutable) child node write its data, and collect the child nodes.
                // The offsets in the Node objects are absolute positions in the encoded output,
                // except for ones that are bitmaps.
                for (unsigned i = 0; i < n; ++i)
                    nodes[i] = _children[i].writeTo(enc);

                // Convert the Nodes' absolute positions into offsets:
                const offset_t childrenPos = (offset_t)enc.nextWritePos();
                auto curPos = childrenPos;
                for (unsigned i = 0; i < n; ++i) {
                    auto &node = nodes[i];
                    if (_children[i].isLeaf())
                        node.leaf.makeRelativeTo(curPos);
                    else
                        node.interior.makeRelativeTo(curPos);
                    curPos += sizeof(nodes[i]);
                }

                // Write the list of children, and return its position & my bitmap:
                enc.writeRaw({nodes, n * sizeof(nodes[0])});
                return Interior(bitmap_t(_bitmap), childrenPos);
            }


            offset_t writeRootTo(Encoder &enc) {
                auto intNode = writeTo(enc);
                auto curPos = (offset_t)enc.nextWritePos();
                intNode.makeRelativeTo(curPos);
                enc.writeRaw({&intNode, sizeof(intNode)});
                return offset_t(curPos);
            }


            void dump(std::ostream &out, unsigned indent =1) const {
                unsigned n = childCount();
                out << string(2*indent, ' ') << "{";
                for (unsigned i = 0; i < n; ++i) {
                    out << "\n";
                    _children[i].dump(out, indent+1);
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
                node = node->addChild(childBitNo, childLeaf);
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
            NodeRef _children[0];           // Variable-size array; size is given by _capacity
        };


#pragma mark - NODEREF METHODS


        bool NodeRef::isLeaf() const {
            return isMutable() ? _asMutable()->isLeaf() : _asImmutable()->isLeaf();
        }

        hash_t NodeRef::hash() const {
            assert(isLeaf());
            return isMutable() ? ((MLeafNode*)_asMutable())->_hash : _asImmutable()->leaf.hash();
        }

        bool NodeRef::matches(Target target) const {
            assert(isLeaf());
            return isMutable() ? ((MLeafNode*)_asMutable())->matches(target)
                               : _asImmutable()->leaf.matches(target.key);
        }

        Node NodeRef::writeTo(Encoder &enc) {
            Node node;
            if (isMutable()) {
                auto mchild = asMutable();
                if (mchild->isLeaf())
                    node.leaf = ((MLeafNode*)mchild)->writeTo(enc);
                else
                    node.interior = ((MInteriorNode*)mchild)->writeTo(enc);
            } else {
                auto ichild = asImmutable();
                if (ichild->isLeaf())
                    node.leaf = ichild->leaf.writeTo(enc);
                else
                    node.interior = ichild->interior.writeTo(enc);
            }
            return node;
        }

        void NodeRef::dump(ostream &out, unsigned indent) const {
            if (isMutable())
                isLeaf() ? ((MLeafNode*)_asMutable())->dump(out, indent)
                         : ((MInteriorNode*)_asMutable())->dump(out, indent);
            else
                isLeaf() ? _asImmutable()->leaf.dump(out, indent)
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
            _root->deleteTree();
    }

    MHashTree& MHashTree::operator= (MHashTree &&other) {
        _imRoot = other._imRoot;
        if (_root)
            _root->deleteTree();
        _root = other._root;
        other._imRoot = nullptr;
        other._root = nullptr;
        return *this;
    }

    MHashTree& MHashTree::operator= (const HashTree *imTree) {
        _imRoot = imTree;
        if (_root)
            _root->deleteTree();
        _root = nullptr;
        return *this;
    }

    unsigned MHashTree::count() const {
        if (_root)
            return _root->leafCount();
        else if (_imRoot)
            return _imRoot->count();
        else
            return 0;
    }

    const Value* MHashTree::get(slice key) const {
        if (_root) {
            Target target(key);
            NodeRef leaf = _root->findNearest(target.hash);
            if (leaf) {
                if (leaf.isMutable()) {
                    auto mleaf = (MLeafNode*)leaf.asMutable();
                    if (mleaf->matches(target))
                        return mleaf->_value;
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

    void MHashTree::insert(slice key, const Value* val) {
        if (!_root)
            _root = MInteriorNode::newRoot(_imRoot);
        _root = _root->insert(Target(key), val, 0);
    }

    bool MHashTree::remove(slice key) {
        if (!_root) {
            if (!_imRoot)
                return false;
            _root = MInteriorNode::newRoot(_imRoot);
        }
        return _root->remove(Target(key), 0);
    }

    uint32_t MHashTree::writeTo(Encoder &enc) {
        if (_root)
            return _root->writeRootTo(enc);
        else
            abort(); //TODO
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
