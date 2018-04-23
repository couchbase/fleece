//
//  MHashTree.hh
//  Fleece
//  Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "slice.hh"
#include "Value.hh"
#include <memory>

namespace fleece {

    namespace mhashtree {
        template <class Key, class Val> class InteriorNode;
    }


    template <class Key, class Val>
    class MHashTree {
    public:
        MHashTree();
        ~MHashTree();

        void insert(const Key&, Val);
        bool remove(const Key&);

        Val get(const Key&) const;

        unsigned count() const;

        void dump(std::ostream &out);

    private:
        std::unique_ptr<mhashtree::InteriorNode<Key, Val>> _root;
    };
}
