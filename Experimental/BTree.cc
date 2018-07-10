//
// BTree.cc
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

#include "BTree.hh"
#include "Dict.hh"
#include <ostream>

using namespace std;


namespace fleece {

    namespace btree {

        /*
         A B-tree node is encoded as a Fleece collection.
         A leaf node is a Dict, mapping keys to values.
         An interior node is an Array, with an odd number of items:
            [c0, k0, c1, k1, ... cn, kn, cn+1]
            where each "c" is a reference to a child node, and each "k" is a key string.
            Each child node ci (i < n) contains only keys less than ki.
            Each child node ci (i > 0) contains only keys greater than or equal to k(i-1).
         */

        uint32_t find(const Array *node, slice key) {
            Array::iterator i(node);
            uint32_t begin = 0, end = i.count() - 1;
            do {
                assert((begin & 1) == 0 && (end & 1) == 0);
                uint32_t mid = ((begin + end) >> 1) | 1;
                slice midKey = i[mid]->asString();
                int cmp = key.compare(midKey);
                if (cmp < 0)
                    end = mid - 1;
                else {
                    begin = mid + 1;
                    if (_usuallyFalse(cmp == 0))
                        break;
                }
            } while (begin < end);
            return begin;
        }

        static unsigned leafCount(const Value *node) {
            auto interior = node->asArray();
            if (interior) {
                unsigned count = 0;
                // A slightly weird loop to iterate over only the even indices:
                auto n = (interior->count() + 1) / 2;
                for (Array::iterator i(interior); ; i += 2) {
                    count += leafCount(i.value());      // TODO: Make this non-recursive
                    if (--n == 0)
                        break;
                }
                return count;
            } else {
                return ((const Dict*)node)->count();
            }
        }

        static void dump(const Value *node, ostream &out, unsigned indent) {
            auto interior = node->asArray();
            out << string(2*indent, ' ');
            if (interior) {
                out << "[\n";
                bool isChild = true;
                for (Array::iterator i(interior); i; ++i) {
                    if (isChild) {
                        dump(i.value(), out, indent + 2);
                    } else {
                        out << "\n" << string(2*indent+1, ' ') << "\"";
                        slice key = i.value()->asString();
                        out.write((char*)key.buf, key.size);
                        out << "\"\n";
                    }
                    isChild = !isChild;
                }
                out << " ]";
            } else {
                out << "(" << ((const Dict*)node)->count() << ") " << node->toJSONString();
            }
        }

    }


    BTree BTree::fromData(slice data) {
        auto root = Value::fromData(data);
        if (!root)
            FleeceException::_throw(InvalidData, "Invalid Fleece data for BTree");
        return BTree(root);
    }

    const Value* BTree::get(slice key) const {
        const Value *node = _root;
        while (node->type() == kArray) {
            auto i = btree::find((const Array*)node, key);
            node = ((const Array*)node)->get(i);
        }
        return ((const Dict*)node)->get(key);
    }

    unsigned BTree::count() const {
        return btree::leafCount(_root);
    }

    void BTree::dump(ostream &out) const {
        out << "BTree [\n";
        btree::dump(_root, out, 1);
        out << "]\n";
    }


}
