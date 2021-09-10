//
// NodeRef.cc
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "NodeRef.hh"
#include "MutableNode.hh"
#include "betterassert.hh"

namespace fleece { namespace hashtree {

    bool NodeRef::isLeaf() const {
        return isMutable() ? _asMutable()->isLeaf() : _asImmutable()->isLeaf();
    }

    hash_t NodeRef::hash() const {
        assert_precondition(isLeaf());
        return isMutable() ? ((MutableLeaf*)_asMutable())->_hash : _asImmutable()->leaf.hash();
    }

    Value NodeRef::value() const {
        assert_precondition(isLeaf());
        return isMutable() ? ((MutableLeaf*)_asMutable())->_value : _asImmutable()->leaf.value();
    }

    bool NodeRef::matches(Target target) const {
        assert_precondition(isLeaf());
        return isMutable() ? ((MutableLeaf*)_asMutable())->matches(target)
                           : _asImmutable()->leaf.matches(target.key);
    }

    unsigned NodeRef::childCount() const {
        assert_precondition(!isLeaf());
        return isMutable() ? ((MutableInterior*)_asMutable())->childCount()
                           : _asImmutable()->interior.childCount();
    }

    NodeRef NodeRef::childAtIndex(unsigned index) const {
        assert_precondition(!isLeaf());
        return isMutable() ? ((MutableInterior*)_asMutable())->childAtIndex(index)
                           : _asImmutable()->interior.childAtIndex(index);
    }


    Node NodeRef::writeTo(Encoder &enc) {
        assert_precondition(!isLeaf());
        Node node;
        if (isMutable())
            node.interior = ((MutableInterior*)asMutable())->writeTo(enc);
        else
            node.interior = asImmutable()->interior.writeTo(enc);
        return node;
    }

    uint32_t NodeRef::writeTo(Encoder &enc, bool writeKey) {
        assert_precondition(isLeaf());
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

} } 
