//
//  HAMTree.hh
//  Fleece
//  Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "slice.hh"
#include "Value.hh"
#include <memory>

namespace fleece {

    namespace hamtree {
        template <class Key, class Val> class InteriorNode;
    }


    template <class Key, class Val>
    class HAMTree {
    public:
        HAMTree();
        ~HAMTree();

        void insert(const Key&, Val);
        bool remove(const Key&);

        Val get(const Key&) const;

        unsigned count() const;

        void dump(std::ostream &out);

    private:
        std::unique_ptr<hamtree::InteriorNode<Key, Val>> _root;
    };
}
