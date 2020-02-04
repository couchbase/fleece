//
// NodeRef.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "MutableHashTree.hh"
#include "HashTree.hh"
#include "HashTree+Internal.hh"
#include <ostream>
#include "betterassert.hh"

namespace fleece { namespace hashtree {
    class MutableNode;


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
        NodeRef(MutableNode* n)                 :_addr(size_t(n) | 1) {assert_precondition(n);}
        NodeRef(const Node* n)                  :_addr(size_t(n)) {}
        NodeRef(const Leaf* n)                  :_addr(size_t(n)) {}
        NodeRef(const Interior* n)              :_addr(size_t(n)) {}

        void reset()                            {_addr = 0;}

        operator bool () const PURE                  {return _addr != 0;}

        bool isMutable() const PURE                  {return (_addr & 1) != 0;}

        MutableNode* asMutable() const PURE {
            return isMutable() ? _asMutable() : nullptr;
        }

        const Node* asImmutable() const PURE {
            return isMutable() ? nullptr : _asImmutable();
        }

        bool isLeaf() const PURE;
        hash_t hash() const PURE;
        bool matches(Target) const PURE;
        Value value() const PURE;

        unsigned childCount() const PURE;
        NodeRef childAtIndex(unsigned index) const PURE;

        Node writeTo(Encoder &enc);
        uint32_t writeTo(Encoder &enc, bool writeKey);
        void dump(std::ostream&, unsigned indent) const;

    private:
        MutableNode* _asMutable() const         {return (MutableNode*)(_addr & ~1);}
        const Node* _asImmutable() const        {return (const Node*)_addr;}

        size_t _addr;
    };

} } 
