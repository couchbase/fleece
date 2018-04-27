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
    class Encoder;
    namespace hashtree {
        class MInterior;
        class NodeRef;
    }


    class MHashTree {
    public:
        MHashTree();
        MHashTree(const HashTree*);
        ~MHashTree();

        MHashTree& operator= (MHashTree&&);
        MHashTree& operator= (const HashTree*);

        void insert(slice key, const Value*);
        bool remove(slice key);

        const Value* get(slice key) const;

        unsigned count() const;

        bool isChanged() const                  {return _root != nullptr;}

        uint32_t writeTo(Encoder&);

        void dump(std::ostream &out);

        using iterator = HashTree::iterator;
        
    private:
        hashtree::NodeRef rootNode() const;

        const HashTree* _imRoot {nullptr};
        hashtree::MInterior* _root {nullptr};

        friend class HashTree::iterator;
    };
}
