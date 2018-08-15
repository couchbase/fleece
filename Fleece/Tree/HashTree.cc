//
//  HashTree.cc
//
// Copyright © 2018 Couchbase. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "HashTree.hh"
#include "HashTree+Internal.hh"
#include "Bitmap.hh"
#include "Endian.hh"
#include <algorithm>
#include <ostream>
#include <string>
#include "TempArray.hh"

using namespace std;

namespace fleece {

    // The `offset` type is interpreted as a little-endian offset down from the containing object.
    #define deref(OFF, TYPE)    ((const TYPE*)((uint8_t*)this - (OFF)))

    namespace hashtree {

        const Value* Leaf::key() const        {return deref(_keyOffset, Value);}
        const Value* Leaf::value() const      {return deref(_valueOffset & ~1, Value);}
        slice Leaf::keyString() const         {return deref(_keyOffset, Value)->asString();}

        uint32_t Leaf::writeTo(Encoder &enc, bool writeKey) const {
            if (enc.base().contains(this)) {
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
            char hashStr[30];
            sprintf(hashStr, "[%08x ", hash());
            out << string(2*indent, ' ') << hashStr << '"';
            auto k = keyString();
            out.write((char*)k.buf, k.size);
            out << "\"=" << value()->toJSONString() << "]";
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
            if (enc.base().contains(this)) {
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

    const Value* HashTree::get(slice key) const {
        auto root = rootNode();
        auto leaf = root->findNearest(key.hash());
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
