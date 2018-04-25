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
#include "Writer.hh"
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
        class MNode;


        // Just a key and its hash
        struct Target {
            explicit Target(slice k) :key(k), hash(k.hash()) { }

            bool operator== (const Target &b) const {
                return hash == b.hash && key == b.key;
            }

            slice const key;
            hash_t const hash;
        };


        static void encodeOffset(offset &o, size_t curPos) {
            o = _encLittle32(offset(curPos - o));
        }

        // Pointer to any type of node. Mutable nodes are tagged by setting the LSB.
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

            pair<offset,offset> writeTo(Writer &writer) {
                // Write key:
                auto keyPos = writer.length();
                assert(_key.size < 16);
                uint8_t header = 0x40 | (_key.size & 0x0F);     //FIX: Fake Fleece string header
                writer.write(&header, 1);
                writer.write(_key);
                writer.padToEvenLength();

                // Write value (just as an integer for now) //FIX
                auto valPos = writer.length();
                assert(_value->isInteger());
                auto val = _value->asInt();
                assert(val >= -0x7FF && val <= 0x7FF);
                uint8_t buf[2] = { uint8_t(val >> 8), uint8_t(val & 0xFF) };    //FIX: Fake Fleece int
                writer.write(buf, sizeof(buf));

                return {offset(keyPos), offset(valPos)};
            }

            static void encodeOffsets(pair<offset,offset> &offsets, size_t curPos) {
                encodeOffset(offsets.first, curPos);
                encodeOffset(offsets.second, curPos);
                *(uint8_t*)&offsets.second |= 1;        // tag to denote a leaf
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


            pair<offset,offset> writeTo(Writer &writer) {
                unsigned n = childCount();
                pair<offset,offset> children[n];

                // Let each interior-node child write its children array:
                for (unsigned i = 0; i < n; ++i) {
                    auto child = _children[i];
                    if (child.isMutable()) {
                        if (child.isLeaf())
                            children[i] = ((MLeafNode*)child.asMutable())->writeTo(writer);
                        else
                            children[i] = ((MInteriorNode*)child.asMutable())->writeTo(writer);
                    } else {
                        abort(); //TODO
                    }
                }

                // Convert positions into offsets:
                const auto childrenPos = writer.length();
                auto curPos = childrenPos;
                for (unsigned i = 0; i < n; ++i) {
                    if (_children[i].isLeaf())
                        MLeafNode::encodeOffsets(children[i], curPos);
                    else
                        MInteriorNode::encodeOffsets(children[i], curPos);
                    curPos += sizeof(children[i]);
                }
                writer.write(children, n * sizeof(children[0]));

                return {bitmap_t(_bitmap), childrenPos};
            }

            static void encodeOffsets(pair<offset,offset> &offsets, size_t curPos) {
                offsets.first = _encLittle32(offsets.first);    // This is a bitmap not an offset!
                encodeOffset(offsets.second, curPos);
            }

            offset writeRootTo(Writer &writer) {
                auto offsets = writeTo(writer);
                size_t curPos = writer.length();
                encodeOffsets(offsets, curPos);
                writer.write(&offsets, sizeof(offsets));
                return offset(curPos);
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

        bool NodeRef::matches(Target target) const {
            assert(isLeaf());
            return isMutable() ? ((MLeafNode*)_asMutable())->matches(target)
                               : _asImmutable()->leaf.matches(target.key);
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

    offset MHashTree::writeTo(Writer &writer) {
        if (_root)
            return _root->writeRootTo(writer);
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
