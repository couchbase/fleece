//
//  HashTree.cc
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "HashTree.hh"
#include "HashTree+Internal.hh"
#include "Bitmap.hh"
#include "Endian.hh"
#include "PlatformCompat.hh"
#include "TempArray.hh"
#include <algorithm>
#include <ostream>
#include <string>
#include "betterassert.hh"

using namespace std;

namespace fleece {

    // The `offset` type is interpreted as a little-endian offset down from the containing object.
    #define deref(OFF, TYPE)    ((const TYPE*)((uint8_t*)this - (OFF)))
    #define derefValue(OFF)     Value((FLValue)deref(OFF, void))

    namespace hashtree {


        FLPURE hash_t ComputeHash(slice s) noexcept {
            // FNV-1a hash function.
            // <https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function#FNV-1a_hash>
            auto byte = (const uint8_t*)s.buf;
            uint32_t h = 2166136261;
            for (size_t i = 0; i < s.size; i++, byte++) {
                h = (h ^ *byte) * 16777619;
            }
            return h;
        }

        void Leaf::validate() const {
            assert(_keyOffset > 0);
            assert(_valueOffset > 0);
        }

        Value Leaf::key() const                 {return derefValue(_keyOffset);}
        Value Leaf::value() const               {return derefValue(_valueOffset & ~1);}
        slice Leaf::keyString() const           {return derefValue(_keyOffset).asString();}

        uint32_t Leaf::writeTo(Encoder &enc, bool writeKey) const {
            if (enc.base().containsAddress(this)) {
                auto pos = int32_t((char*)this - (char*)enc.base().end());
                return pos - (writeKey ? _keyOffset : _valueOffset);
            } else {
                if (writeKey)
                    enc.writeValue(key());
                else
                    enc.writeValue(value());
                return (uint32_t)enc.finishItem();
            }
        }

        void Leaf::dump(std::ostream &out, unsigned indent) const {
            constexpr size_t bufSize = 30;
            char hashStr[bufSize];
            snprintf(hashStr, bufSize, "[%08x ", hash());
            out << string(2*indent, ' ') << hashStr << '"';
            auto k = keyString();
            out.write((char*)k.buf, k.size);
            out << "\"=" << value().toJSONString() << "]";
        }

        
        void Interior::validate() const {
            assert(_childrenOffset > 0);
        }

        bitmap_t Interior::bitmap() const             {return endian::decLittle32(_bitmap);}

        bool Interior::hasChild(unsigned bitNo) const {return asBitmap(bitmap()).containsBit(bitNo);}
        unsigned Interior::childCount() const         {return asBitmap(bitmap()).bitCount();}

        const Node* Interior::childAtIndex(int i) const {
            assert_precondition(_childrenOffset > 0);
            return (deref(_childrenOffset, Node) + i)->validate();
        }

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
            auto child = childAtIndex(0);
            for (unsigned i = 0; i < n; ++i, ++child) {
                out << "\n";
                if (child->isLeaf())
                    child->leaf.dump(out, indent+1);
                else
                    child->interior.dump(out, indent+1);
            }
            out << " ]";
        }

        Interior Interior::writeTo(Encoder &enc) const {
            if (enc.base().containsAddress(this)) {
                auto pos = int32_t((char*)this - (char*)enc.base().end());
                return makeAbsolute(pos);
            } else {
                //FIX: DRY FAIL: This is nearly identical to MInteriorNode::writeTo()
                unsigned n = childCount();
                TempArray(nodes, Node, n);
                for (unsigned i = 0; i < n; ++i) {
                    auto child = childAtIndex(i);
                    if (!child->isLeaf())
                        nodes[i].interior = child->interior.writeTo(enc);
                }
                for (unsigned i = 0; i < n; ++i) {
                    auto child = childAtIndex(i);
                    if (child->isLeaf())
                        nodes[i].leaf._valueOffset = child->leaf.writeTo(enc, false);
                }
                for (unsigned i = 0; i < n; ++i) {
                    auto child = childAtIndex(i);
                    if (child->isLeaf())
                        nodes[i].leaf._keyOffset = child->leaf.writeTo(enc, true);
                }

                const uint32_t childrenPos = (uint32_t)enc.nextWritePos();
                auto curPos = childrenPos;
                for (unsigned i = 0; i < n; ++i) {
                    auto &node = nodes[i];
                    if (childAtIndex(i)->isLeaf())
                        node.leaf.makeRelativeTo(curPos);
                    else
                        node.interior.makeRelativeTo(curPos);
                    curPos += sizeof(nodes[i]);
                }
                enc.writeRaw({nodes, n * sizeof(nodes[0])});
                return Interior(bitmap_t(_bitmap), childrenPos);
            }
        }

    }

    using namespace hashtree;


    const HashTree* HashTree::fromData(slice data) {
        return (const HashTree*)offsetby(data.end(), -(ssize_t)sizeof(Interior));
    }


    const Interior* HashTree::rootNode() const {
        return (const Interior*)this;
    }

    Value HashTree::get(slice key) const {
        auto root = rootNode();
        auto leaf = root->findNearest(ComputeHash(key));
        if (leaf && leaf->keyString() == key)
            return leaf->value();
        return nullptr;
    }

    unsigned HashTree::count() const {
        return rootNode()->leafCount();
    }

    void HashTree::dump(ostream &out) const {
        out << "HashTree [\n";
        rootNode()->dump(out);
        out << "]\n";
    }

}
