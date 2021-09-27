//
//  MutableHashTree.cc
//  Fleece
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "MutableHashTree.hh"
#include "HashTree.hh"
#include "HashTree+Internal.hh"
#include "NodeRef.hh"
#include "MutableNode.hh"
#include "fleece/Mutable.hh"
#include "Bitmap.hh"
#include "HeapArray.hh"
#include "HeapDict.hh"
#include <algorithm>
#include <ostream>
#include <string>
#include "betterassert.hh"

using namespace std;

namespace fleece {
//    using namespace impl::internal;
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

    MutableHashTree& MutableHashTree::operator= (MutableHashTree &&other) noexcept {
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

    Value MutableHashTree::get(slice key) const {
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

    void MutableHashTree::set(slice key, Value val) {
        if (val)
            insert(key, [=](Value){ return val; });
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


    MutableArray MutableHashTree::getMutableArray(slice key) {
        MutableArray result;
        insert(key, [&](Value value) -> Value {
            auto array = value.asArray();
            result = array.asMutable();
            if (!result)
                result = array.mutableCopy();
            return result;
        });
       return result;
    }

    MutableDict MutableHashTree::getMutableDict(slice key) {
        MutableDict result;
        insert(key, [&](Value value) -> Value {
            auto dict = value.asDict();
            result = dict.asMutable();
            if (!result)
                result = dict.mutableCopy();
            return result;
        });
        return result;
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

            pair<slice,Value> next() {
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
