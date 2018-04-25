//
//  MHashTree.hh
//  Fleece
//  Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "HashTree.hh"
#include "slice.hh"

namespace fleece {
    class Writer;
    namespace hashtree {
        class MInteriorNode;
    }


    class MHashTree {
    public:
        MHashTree();
        MHashTree(const HashTree*);
        ~MHashTree();

        void insert(slice key, const Value*);
        bool remove(slice key);

        const Value* get(slice key) const;

        unsigned count() const;

        uint32_t writeTo(Writer&);

        void dump(std::ostream &out);

    private:
        const HashTree* _imRoot {nullptr};
        hashtree::MInteriorNode* _root {nullptr};
    };
}
