//
//  HashTree.hh
//  Fleece
//
//  Created by Jens Alfke on 4/23/18.
//Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "slice.hh"
#include "Value.hh"

namespace fleece {

    class MHashTree;

    namespace hashtree {
        class Interior;
        class MInterior;
        class NodeRef;
        class iteratorImpl;
    }


    /** The root of an immutable tree encoded alongside Fleece data. */
    class HashTree {
    public:
        static const HashTree* fromData(slice data);

        const Value* get(slice) const;

        unsigned count() const;

        void dump(std::ostream &out) const;


        class iterator {
        public:
            iterator(const MHashTree&);
            iterator(const HashTree*);
            ~iterator();
            slice key() const noexcept                      {return _key;}
            const Value* value() const noexcept             {return _value;}
            explicit operator bool() const noexcept         {return _value != nullptr;}
            iterator& operator ++();
        private:
            iterator(hashtree::NodeRef);
            std::unique_ptr<hashtree::iteratorImpl> _impl;
            slice _key;
            const Value *_value;
        };

    private:
        const hashtree::Interior* rootNode() const;

        friend class hashtree::MInterior;
        friend class MHashTree;
    };
}
