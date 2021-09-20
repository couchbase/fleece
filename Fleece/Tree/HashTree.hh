//
//  HashTree.hh
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "fleece/slice.hh"
#include "fleece/Fleece.hh"
#include <memory>

namespace fleece {

    class MutableHashTree;

    namespace hashtree {
        class Interior;
        class MutableInterior;
        class NodeRef;
        struct iteratorImpl;
    }


    /** The root of an immutable tree encoded alongside Fleece data. */
    class HashTree {
    public:
        static const HashTree* fromData(slice data);

        Value get(slice) const;

        unsigned count() const;

        void dump(std::ostream &out) const;


        class iterator {
        public:
            iterator(const MutableHashTree&);
            iterator(const HashTree*);
            iterator(iterator&&);
            ~iterator();
            slice key() const noexcept                      {return _key;}
            Value value() const noexcept                    {return _value;}
            explicit operator bool() const noexcept         {return !!_value;}
            iterator& operator ++();
        private:
            iterator(hashtree::NodeRef);
            std::unique_ptr<hashtree::iteratorImpl> _impl;
            slice _key;
            Value _value;
        };

    private:
        const hashtree::Interior* rootNode() const;

        friend class hashtree::MutableInterior;
        friend class MutableHashTree;
    };
}
