//
//  MutableNode.hh
//  Fleece
//
//  Created by Jens Alfke on 6/22/18.
//  Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "NodeRef.hh"
#include "PlatformCompat.hh"
#include "RefCounted.hh"
#include "fleece/Mutable.hh"
#include "fleece/slice.hh"
#include "TempArray.hh"
#include "betterassert.hh"

namespace fleece { namespace hashtree {
    using namespace std;


    using offset_t = int32_t;


    // Base class of nodes within a MutableHashTree.
    class MutableNode {
    public:
        MutableNode(unsigned capacity)
        :_capacity(int8_t(capacity))
        {
            assert_precondition(capacity <= kMaxChildren);
        }

        bool isLeaf() const FLPURE     {return _capacity == 0;}

        FLPURE static void encodeOffset(offset_t &o, size_t curPos) {
            assert_precondition((ssize_t)curPos > o);
            o = _encLittle32(offset_t(curPos - o));
        }

    protected:
        uint8_t capacity() const FLPURE {
            assert_precondition(_capacity > 0);
            return _capacity;
        }

        int8_t _capacity;
    };


    // A leaf node that holds a single key and value.
    class MutableLeaf : public MutableNode {
    public:
        MutableLeaf(const Target &t, Value v)
        :MutableNode(0)
        ,_key(t.key)
        ,_hash(t.hash)
        ,_value(v)
        { }

        bool matches(Target target) const {
            return _hash == target.hash && _key == target.key;
        }

        uint32_t writeTo(Encoder &enc, bool writeKey) {
            if (writeKey)
                enc.writeString(_key);
            else
                enc.writeValue(_value);
            return (uint32_t)enc.finishItem();
        }

        void dump(std::ostream &out, unsigned indent) {
            char hashStr[30];
            sprintf(hashStr, "{%08x ", _hash);
            out << string(2*indent, ' ') << hashStr << '"';
            out.write((char*)_key.buf, _key.size);
            out << "\"=" << _value.toJSONString() << "}";
        }

        alloc_slice const _key;
        hash_t const _hash;
        RetainedValue _value;
    };


    // An interior node holds a small compact hash table mapping to Nodes.
    class MutableInterior : public MutableNode {
    public:

        static MutableInterior* newRoot(const HashTree *imTree) {
            if (imTree)
                return mutableCopy(imTree->rootNode());
            else
                return newNode(kMaxChildren);
        }


        unsigned childCount() const {
            return _bitmap.bitCount();
        }


        NodeRef childAtIndex(unsigned index) {
            assert_precondition(index < capacity());
            return _children[index];
        }


        void deleteTree() {
            unsigned n = childCount();
            for (unsigned i = 0; i < n; ++i) {
                auto child = _children[i].asMutable();
                if (child) {
                    if (child->isLeaf())
                        delete (MutableLeaf*)child;
                    else
                        ((MutableInterior*)child)->deleteTree();
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
                        count += ((MutableInterior*)child.asMutable())->leafCount();
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
                return ((MutableInterior*)mchild)->findNearest(hash >> kBitShift);  // recurse...
            } else {
                auto ichild = child.asImmutable();
                return ichild->interior.findNearest(hash >> kBitShift);
            }
        }


        // Recursive insertion method. On success returns either 'this', or a new node that
        // replaces 'this'. On failure (i.e. callback returned nullptr) returns nullptr.
        MutableInterior* insert(const Target &target, unsigned shift) {
            assert_precondition(shift + kBitShift < 8*sizeof(hash_t));//FIX: //TODO: Handle hash collisions
            unsigned bitNo = childBitNumber(target.hash, shift);
            if (!hasChild(bitNo)) {
                // No child -- add a leaf:
                Value val = (*target.insertCallback)(nullptr);
                if (!val)
                    return nullptr;
                return addChild(bitNo, new MutableLeaf(target, val));
            }
            NodeRef &childRef = childForBitNumber(bitNo);
            if (childRef.isLeaf()) {
                if (childRef.matches(target)) {
                    // Leaf node matches this key; update or copy it:
                    Value val = (*target.insertCallback)(childRef.value());
                    if (!val)
                        return nullptr;
                    if (childRef.isMutable())
                        ((MutableLeaf*)childRef.asMutable())->_value = val;
                    else
                        childRef = new MutableLeaf(target, val);
                    return this;
                } else {
                    // Nope, need to promote the leaf to an interior node & add new key:
                    MutableInterior *node = promoteLeaf(childRef, shift);
                    auto insertedNode = node->insert(target, shift+kBitShift);
                    if (!insertedNode) {
                        delete node;
                        return nullptr;
                    }
                    childRef = insertedNode;
                    return this;
                }
            } else {
                // Progress down to interior node...
                auto child = (MutableInterior*)childRef.asMutable();
                if (!child)
                    child = mutableCopy(&childRef.asImmutable()->interior, 1);
                child = child->insert(target, shift+kBitShift);
                if (child)
                    childRef = child;
                //FIX: This can leak if child is created by mutableCopy, but then
                return this;
            }
        }


        bool remove(Target target, unsigned shift) {
            assert_precondition(shift + kBitShift < 8*sizeof(hash_t));
            unsigned bitNo = childBitNumber(target.hash, shift);
            if (!hasChild(bitNo))
                return false;
            unsigned childIndex = childIndexForBitNumber(bitNo);
            NodeRef childRef = _children[childIndex];
            if (childRef.isLeaf()) {
                // Child is a leaf -- is it the right key?
                if (childRef.matches(target)) {
                    removeChild(bitNo, childIndex);
                    delete (MutableLeaf*)childRef.asMutable();
                    return true;
                } else {
                    return false;
                }
            } else {
                // Recurse into child node...
                auto child = (MutableInterior*)childRef.asMutable();
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

            // `nodes` is an in-memory staging area for the child nodes I'll write.
            // The offsets in it are absolute positions in the encoded output,
            // except for ones that are bitmaps.
            TempArray(nodes, Node, n);

            // Write interior nodes, then leaf node Values, then leaf node keys.
            // This keeps the keys near me, for better locality of reference.
            for (unsigned i = 0; i < n; ++i) {
                if (!_children[i].isLeaf())
                    nodes[i] = _children[i].writeTo(enc);
            }
            for (unsigned i = 0; i < n; ++i) {
                if (_children[i].isLeaf())
                    nodes[i].leaf._valueOffset = _children[i].writeTo(enc, false);
            }
            for (unsigned i = 0; i < n; ++i) {
                if (_children[i].isLeaf())
                    nodes[i].leaf._keyOffset = _children[i].writeTo(enc, true);
            }

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

        static void operator delete(void* ptr) {
            ::operator delete(ptr);
        }

    private:
        MutableInterior() = delete;
        MutableInterior(const MutableInterior& i) = delete;
        MutableInterior(MutableInterior&& i) = delete;
        MutableInterior& operator=(const MutableInterior&) = delete;

        static MutableInterior* newNode(unsigned capacity, MutableInterior *orig =nullptr) {
            return new (capacity) MutableInterior(capacity, orig);
        }

        static void* operator new(size_t size, unsigned capacity) {
            return ::operator new(size + capacity*sizeof(NodeRef));
        }

        static void operator delete(void* ptr, unsigned capacity) {
            ::operator delete(ptr);
        }

        static MutableInterior* mutableCopy(const Interior *iNode, unsigned extraCapacity =0) {
            auto childCount = iNode->childCount();
            auto node = newNode(childCount + extraCapacity);
            node->_bitmap = asBitmap(iNode->bitmap());
            for (unsigned i = 0; i < childCount; ++i)
                node->_children[i] = NodeRef(iNode->childAtIndex(i));
            return node;
        }

        static MutableInterior* promoteLeaf(NodeRef& childLeaf, unsigned shift) {
            unsigned level = shift / kBitShift;
            MutableInterior* node = newNode(2 + (level<1) + (level<3));
            unsigned childBitNo = childBitNumber(childLeaf.hash(), shift+kBitShift);
            node = node->addChild(childBitNo, childLeaf);
            return node;
        }

        MutableInterior(unsigned cap, MutableInterior* orig =nullptr)
        :MutableNode(cap)
        ,_bitmap(orig ? orig->_bitmap : Bitmap<bitmap_t>{})
        {
            if (orig)
                memcpy(_children, orig->_children, orig->capacity()*sizeof(NodeRef));
            else
                memset(_children, 0, cap*sizeof(NodeRef));
        }


        MutableInterior* grow() {
            assert_precondition(capacity() < kMaxChildren);
            auto replacement = (MutableInterior*)realloc(this,
                                    sizeof(MutableInterior) + (capacity()+1)*sizeof(NodeRef));
            if (!replacement)
                throw std::bad_alloc();
            replacement->_capacity++;
            return replacement;
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
            return const_cast<MutableInterior*>(this)->childForBitNumber(bitNo);
        }


        MutableInterior* addChild(unsigned bitNo, NodeRef child) {
            return addChild(bitNo, childIndexForBitNumber(bitNo), child);
        }

        MutableInterior* addChild(unsigned bitNo, unsigned childIndex, NodeRef child) {
            MutableInterior* node = (childCount() < capacity()) ? this : grow();
            return node->_addChild(bitNo, childIndex, child);
        }

        MutableInterior* _addChild(unsigned bitNo, unsigned childIndex, NodeRef child) {
            assert_precondition(child);
            memmove(&_children[childIndex+1], &_children[childIndex],
                    (capacity() - childIndex - 1)*sizeof(NodeRef));
            _children[childIndex] = child;
            _bitmap.addBit(bitNo);
            return this;
        }

        void removeChild(unsigned bitNo, unsigned childIndex) {
            assert_precondition(childIndex < capacity());
            memmove(&_children[childIndex], &_children[childIndex+1], (capacity() - childIndex - 1)*sizeof(NodeRef));
            _bitmap.removeBit(bitNo);
        }


        Bitmap<bitmap_t> _bitmap {0};
#ifdef _MSC_VER
#pragma warning(push)
// warning C4200: nonstandard extension used: zero-sized array in struct/union
// note: This member will be ignored by a defaulted constructor or copy/move assignment operator
// Jim: So keep the default copy, move, and constructor deleted, and use care if they are overriden
#pragma warning(disable : 4200)
#endif
        NodeRef _children[0];           // Variable-size array; size is given by _capacity
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    };

} }
