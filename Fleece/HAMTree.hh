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

    using Key = alloc_slice;
    using Val = int;

    namespace hamtree {
        class InteriorNode;
    }

    class HAMTree {
    public:
        HAMTree();
        ~HAMTree();

        void insert(Key, Val);
        bool remove(Key);

        Val get(Key) const;

        unsigned count() const;

        void dump(std::ostream &out);

    private:
        std::unique_ptr<hamtree::InteriorNode> _root;
    };
}
