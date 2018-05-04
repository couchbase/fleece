//
//  MHashTree.hh
//  Fleece
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "HashTree.hh"
#include "slice.hh"
#include <functional>
#include <memory>

namespace fleece {
    class Encoder;
    namespace hashtree {
        class MutableInterior;
        class NodeRef;
    }


    class MutableHashTree {
    public:
        MutableHashTree();
        MutableHashTree(const HashTree*);
        ~MutableHashTree();

        MutableHashTree& operator= (MutableHashTree&&);
        MutableHashTree& operator= (const HashTree*);

        using InsertCallback = std::function<const Value*(const Value*)>;

        void insert(slice key, const Value*);
        bool insert(slice key, InsertCallback);
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
        hashtree::MutableInterior* _root {nullptr};

        friend class HashTree::iterator;
    };
}
