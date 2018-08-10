//
// NodeRef.cc
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

#include "NodeRef.hh"
#include "MutableNode.hh"

namespace fleece { namespace impl { namespace hashtree {

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

} } }
