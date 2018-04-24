//
//  HashTree.cc
//  Fleece
//
//  Created by Jens Alfke on 4/23/18.
//Copyright © 2018 Couchbase. All rights reserved.
//

#include "HashTree.hh"
#include "Bitmap.hh"
#include "Endian.hh"
#include <algorithm>
#include <ostream>
#include <string>

using namespace std;

namespace fleece {

    // The `offset` type is interpreted as a little-endian offset down from the containing object.
    #define deref(OFF, TYPE)    ((const TYPE*)((uint8_t*)this - _decLittle32(OFF)))

    namespace hashtree {

        const Value* Leaf::key() const        {return deref(_keyOffset, Value);}
        const Value* Leaf::value() const      {return deref(_keyOffset, Value);}
        slice Leaf::keyString() const         {return deref(_valueOffset & ~1, Value)->asString();}

        void Leaf::dump(std::ostream &out) const {
            char str[30];
            sprintf(str, " [%08x]", hash());
            out << str;
        }

        bitmap_t Interior::bitmap() const     {return _decLittle32(_bitmap);}

        bool Interior::hasChild(unsigned bitNo) const {return asBitmap(bitmap()).containsBit(bitNo);}
        unsigned Interior::childCount() const         {return asBitmap(bitmap()).bitCount();}
        const Node* Interior::childAtIndex(int i) const {return deref(_childrenOffset, Node) + i;}

        const Node* Interior::childForBitNumber(unsigned bitNo) const {
            return hasChild(bitNo) ? childAtIndex( asBitmap(bitmap()).indexOfBit(bitNo) ) : nullptr;
        }

        // Finds the leaf node that's closest to the given hash. May not be exact.
        const Leaf* Interior::findNearest(hash_t hash) const {
            const Node *child = childForBitNumber( hash & (kMaxChildren - 1) );
            if (!child)
                return nullptr;
            else if (child->isLeaf())
                return (const Leaf*)child;    // closest match; not guaranteed to have right hash
            else
                return ((const Interior*)child)->findNearest(hash >> kBitShift);  // recurse...
        }

        // Returns the total number of leaves under this node.
        unsigned Interior::leafCount() const {
            unsigned count = 0;
            auto c = childAtIndex(0);
            for (unsigned n = childCount(); n > 0; --n, ++c) {
                if (c->isLeaf())
                    count += 1;
                else
                    count += ((Interior*)c)->leafCount();
            }
            return count;
        }

        void Interior::dump(std::ostream &out, unsigned indent =1) const {
            unsigned n = childCount();
            out << string(2*indent, ' ') << "[";
            unsigned leafCount = n;
            auto child = childAtIndex(0);
            for (unsigned i = 0; i < n; ++i, ++child) {
                if (!child->isLeaf()) {
                    --leafCount;
                    out << "\n";
                    child->interior.dump(out, indent+1);
                }
            }
            if (leafCount > 0) {
                if (leafCount < n)
                    out << "\n" << string(2*indent, ' ') << " ";
                child = childAtIndex(0);
                for (unsigned i = 0; i < n; ++i, ++child) {
                    if (child->isLeaf())
                        child->leaf.dump(out);
                }
            }
            out << " ]";
        }

    }

    using namespace hashtree;


    const Interior* HashTree::getRoot() const {
        return _rootOffset ? deref(_rootOffset, hashtree::Interior) : nullptr;
    }

    const Value* HashTree::get(Key key) const {
        auto root = getRoot();
        if (root) {
            hash_t hash = (hash_t)std::hash<Key>()(key);
            auto leaf = root->findNearest(hash);
            if (leaf && leaf->keyString() == key)
                return leaf->value();
        }
        return nullptr;
    }

    unsigned HashTree::count() const {
        auto root = getRoot();
        return root ? root->leafCount() : 0;
    }

    void HashTree::dump(ostream &out) const {
        auto root = getRoot();
        out << "HashTree {";
        if (root) {
            out << "\n";
            root->dump(out);
        }
        out << "}\n";
    }

}
