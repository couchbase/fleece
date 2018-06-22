//
//  MutableHashTree.cc
//  Fleece
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#include "MutableHashTree.hh"
#include "HashTree.hh"
#include "HashTree+Internal.hh"
#include "Bitmap.hh"
#include "Encoder.hh"
#include "HeapArray.hh"
#include "HeapDict.hh"
#include <algorithm>
#include <ostream>
#include <string>

using namespace std;

namespace fleece {
    using namespace internal;

    namespace hashtree {
        class MutableNode;


        using offset_t = int32_t;


        // Specifies an insertion/deletion
        struct Target {
            explicit Target(slice k, MutableHashTree::InsertCallback *callback =nullptr)
            :key(k), hash(k.hash()), insertCallback(callback)
            { }

            bool operator== (const Target &b) const {
                return hash == b.hash && key == b.key;
            }

            slice const key;
            hash_t const hash;
            MutableHashTree::InsertCallback *insertCallback {nullptr};
        };


        // Holds a pointer to any type of node. Mutable nodes are tagged by setting the LSB.
        class NodeRef {
        public:
            NodeRef()                               :_addr(0) { }
            NodeRef(MutableNode* n)                       :_addr(size_t(n) | 1) {assert(n);}
            NodeRef(const Node* n)                  :_addr(size_t(n)) {}
            NodeRef(const Leaf* n)                  :_addr(size_t(n)) {}
            NodeRef(const Interior* n)              :_addr(size_t(n)) {}

            void reset()                            {_addr = 0;}

            operator bool () const                  {return _addr != 0;}

            bool isMutable() const                  {return (_addr & 1) != 0;}

            MutableNode* asMutable() const {
                return isMutable() ? _asMutable() : nullptr;
            }

            const Node* asImmutable() const {
                return isMutable() ? nullptr : _asImmutable();
            }

            bool isLeaf() const;
            hash_t hash() const;
            bool matches(Target) const;
            const Value* value() const;

            unsigned childCount() const;
            NodeRef childAtIndex(unsigned index) const;

            Node writeTo(Encoder &enc);
            uint32_t writeTo(Encoder &enc, bool writeKey);
            void dump(ostream&, unsigned indent) const;

        private:
            MutableNode* _asMutable() const               {return (MutableNode*)(_addr & ~1);}
            const Node* _asImmutable() const        {return (const Node*)_addr;}

            size_t _addr;
        };


        // Base class of nodes within a MutableHashTree.
        class MutableNode {
        public:
            MutableNode(unsigned capacity)
            :_capacity(int8_t(capacity))
            {
                assert(capacity <= kMaxChildren);
            }

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

            int8_t _capacity;
        };


        // A leaf node that holds a single key and value.
        class MutableLeaf : public MutableNode {
        public:
            MutableLeaf(const Target &t, const Value *v)
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
                out << "\"=" << _value->toJSONString() << "}";
            }

            alloc_slice const _key;
            hash_t const _hash;
            RetainedConst<Value> _value;
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
                assert(index < capacity());
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
                assert(shift + kBitShift < 8*sizeof(hash_t));//FIX: //TODO: Handle hash collisions
                unsigned bitNo = childBitNumber(target.hash, shift);
                if (!hasChild(bitNo)) {
                    // No child -- add a leaf:
                    const Value *val = (*target.insertCallback)(nullptr);
                    if (!val)
                        return nullptr;
                    return addChild(bitNo, new MutableLeaf(target, val));
                }
                NodeRef &childRef = childForBitNumber(bitNo);
                if (childRef.isLeaf()) {
                    if (childRef.matches(target)) {
                        // Leaf node matches this key; update or copy it:
                        const Value *val = (*target.insertCallback)(childRef.value());
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
                assert(shift + kBitShift < 8*sizeof(hash_t));
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
                Node nodes[n];

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


        private:
            static MutableInterior* newNode(unsigned capacity, MutableInterior *orig =nullptr) {
                return new (capacity) MutableInterior(capacity, orig);
            }

            static void* operator new(size_t size, unsigned capacity) {
                return ::operator new(size + capacity*sizeof(NodeRef));
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
                assert(capacity() < kMaxChildren);
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
            return isMutable() ? ((MutableLeaf*)_asMutable())->_hash : _asImmutable()->leaf.hash();
        }

        const Value* NodeRef::value() const {
            assert(isLeaf());
            return isMutable() ? ((MutableLeaf*)_asMutable())->_value.get() : _asImmutable()->leaf.value();
        }

        bool NodeRef::matches(Target target) const {
            assert(isLeaf());
            return isMutable() ? ((MutableLeaf*)_asMutable())->matches(target)
                               : _asImmutable()->leaf.matches(target.key);
        }

        unsigned NodeRef::childCount() const {
            assert(!isLeaf());
            return isMutable() ? ((MutableInterior*)_asMutable())->childCount()
                               : _asImmutable()->interior.childCount();
        }

        NodeRef NodeRef::childAtIndex(unsigned index) const {
            assert(!isLeaf());
            return isMutable() ? ((MutableInterior*)_asMutable())->childAtIndex(index)
                               : _asImmutable()->interior.childAtIndex(index);
        }


        Node NodeRef::writeTo(Encoder &enc) {
            assert(!isLeaf());
            Node node;
            if (isMutable())
                node.interior = ((MutableInterior*)asMutable())->writeTo(enc);
            else
                node.interior = asImmutable()->interior.writeTo(enc);
            return node;
        }

        uint32_t NodeRef::writeTo(Encoder &enc, bool writeKey) {
            assert(isLeaf());
            if (isMutable())
                return ((MutableLeaf*)asMutable())->writeTo(enc, writeKey);
            else
                return asImmutable()->leaf.writeTo(enc, writeKey);
        }

        void NodeRef::dump(ostream &out, unsigned indent) const {
            if (isMutable())
                isLeaf() ? ((MutableLeaf*)_asMutable())->dump(out, indent)
                         : ((MutableInterior*)_asMutable())->dump(out, indent);
            else
                isLeaf() ? _asImmutable()->leaf.dump(out, indent)
                         : _asImmutable()->interior.dump(out, indent);
        }


    } // end namespace


#pragma mark - MutableHashTree ITSELF


    using namespace hashtree;

    MutableHashTree::MutableHashTree()
    { }

    MutableHashTree::MutableHashTree(const HashTree *tree)
    :_imRoot(tree)
    { }

    MutableHashTree::~MutableHashTree() {
        if (_root)
            _root->deleteTree();
    }

    MutableHashTree& MutableHashTree::operator= (MutableHashTree &&other) {
        _imRoot = other._imRoot;
        if (_root)
            _root->deleteTree();
        _root = other._root;
        other._imRoot = nullptr;
        other._root = nullptr;
        return *this;
    }

    MutableHashTree& MutableHashTree::operator= (const HashTree *imTree) {
        _imRoot = imTree;
        if (_root)
            _root->deleteTree();
        _root = nullptr;
        return *this;
    }

    unsigned MutableHashTree::count() const {
        if (_root)
            return _root->leafCount();
        else if (_imRoot)
            return _imRoot->count();
        else
            return 0;
    }

    NodeRef MutableHashTree::rootNode() const {
        if (_root)
            return _root;
        else if (_imRoot)
            return _imRoot->rootNode();
        else
            return {};
    }

    const Value* MutableHashTree::get(slice key) const {
        if (_root) {
            Target target(key);
            NodeRef leaf = _root->findNearest(target.hash);
            if (leaf) {
                if (leaf.isMutable()) {
                    auto mleaf = (MutableLeaf*)leaf.asMutable();
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

    bool MutableHashTree::insert(slice key, InsertCallback callback) {
        if (!_root)
            _root = MutableInterior::newRoot(_imRoot);
        auto result = _root->insert(Target(key, &callback), 0);
        if (!result)
            return false;
        _root = result;
        return true;
    }

    void MutableHashTree::set(slice key, const Value* val) {
        if (val)
            insert(key, [=](const Value*){ return val; });
        else
            remove(key);
    }

    bool MutableHashTree::remove(slice key) {
        if (!_root) {
            if (!_imRoot)
                return false;
            _root = MutableInterior::newRoot(_imRoot);
        }
        return _root->remove(Target(key), 0);
    }


    MutableArray* MutableHashTree::getMutableArray(slice key) {
        return (MutableArray*)getMutable(key, kArrayTag);
    }

    MutableDict* MutableHashTree::getMutableDict(slice key) {
        return (MutableDict*)getMutable(key, kDictTag);
    }

    Value* MutableHashTree::getMutable(slice key, tags ifType) {
        Retained<HeapCollection> result = nullptr;
        insert(key, [&](const Value *value) {
            result = HeapCollection::mutableCopy(value, ifType);
            return result ? result->asValue() : nullptr;
        });
        return (Value*)HeapCollection::asValue(result);
    }


    uint32_t MutableHashTree::writeTo(Encoder &enc) {
        if (_root) {
            return _root->writeRootTo(enc);
        } else if (_imRoot) {
            unique_ptr<MutableInterior> tempRoot( MutableInterior::newRoot(_imRoot) );
            return tempRoot->writeRootTo(enc);
        } else {
            return 0;
        }
    }

    void MutableHashTree::dump(std::ostream &out) {
        if (_imRoot && !_root) {
            _imRoot->dump(out);
        } else {
            out << "MutableHashTree {";
            if (_root) {
                out << "\n";
                _root->dump(out);
            }
            out << "}\n";
        }
    }


#pragma mark - ITERATOR


    namespace hashtree {

        struct iteratorImpl {
            static constexpr size_t kMaxDepth = (8*sizeof(hash_t) + kBitShift - 1) / kBitShift;

            struct pos {
                NodeRef parent;         // Always an interior node
                int index;              // Current child index
            };
            NodeRef node;
            pos current;
            pos stack[kMaxDepth];
            unsigned depth;

            iteratorImpl(NodeRef root)
            :current {root, -1}
            ,depth {0}
            { }

            pair<slice,const Value*> next() {
                while (unsigned(++current.index) >= current.parent.childCount()) {
                    if (depth > 0) {
                        // Pop the stack:
                        current = stack[--depth];
                    } else {
                        node.reset();      // at end
                        return {};
                    }
                }

                // Get the current node:
                while (true) {
                    node = current.parent.childAtIndex(current.index);
                    if (node.isLeaf())
                        break;

                    // If it's an interior node, recurse into its children:
                    assert(depth < kMaxDepth);
                    stack[depth++] = current;
                    current = {node, 0};
                }

                // Return the current leaf's key/value:
                if (node.isMutable()) {
                    auto leaf = ((MutableLeaf*)node.asMutable());
                    return {leaf->_key, leaf->_value};
                } else {
                    auto &leaf = node.asImmutable()->leaf;
                    return {leaf.keyString(), leaf.value()};
                }
            }
        };

    }



    HashTree::iterator::iterator(const MutableHashTree &tree)
    :iterator(tree.rootNode())
    { }

    HashTree::iterator::iterator(const HashTree *tree)
    :iterator(tree->rootNode())
    { }


    HashTree::iterator::iterator(NodeRef root)
    :_impl(new iteratorImpl(root))
    {
        if (!_impl->current.parent)
            _value = nullptr;
        else
            tie(_key, _value) = _impl->next();
    }

    HashTree::iterator::~iterator() =default;

    HashTree::iterator& MutableHashTree::iterator::operator++() {
        tie(_key, _value) = _impl->next();
        return *this;
    }

}
