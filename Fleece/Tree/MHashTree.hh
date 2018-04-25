//
//  MHashTree.hh
//  Fleece
//  Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "HashTree.hh"
#include "slice.hh"
#include <memory>

namespace fleece {
    class Writer;
    namespace hashtree {
        class MInteriorNode;
    }


    class MHashTree {
    public:
        using Key = alloc_slice;
        using Val = const Value*;

        MHashTree();
        MHashTree(const HashTree*);
        ~MHashTree();

        void insert(const Key&, Val);
        bool remove(const Key&);

        Val get(const Key&) const;

        unsigned count() const;

        uint32_t writeTo(Writer&);

        void dump(std::ostream &out);

    private:
        const HashTree* _imRoot {nullptr};
        std::unique_ptr<hashtree::MInteriorNode> _root;
    };
}
