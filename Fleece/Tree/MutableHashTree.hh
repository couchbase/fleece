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
    class MutableArray;
    class MutableDict;
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

        const Value* get(slice key) const;

        MutableArray* getMutableArray(slice key);
        MutableDict* getMutableDict(slice key);

        unsigned count() const;

        bool isChanged() const                  {return _root != nullptr;}

        using InsertCallback = std::function<const Value*(const Value*)>;

        void set(slice key, const Value*);
        bool insert(slice key, InsertCallback);
        bool remove(slice key);

        uint32_t writeTo(Encoder&);

        void dump(std::ostream &out);

        using iterator = HashTree::iterator;
        
    private:
        hashtree::NodeRef rootNode() const;
        internal::MutableCollection* getMutable(slice key, internal::tags ifType);

        const HashTree* _imRoot {nullptr};
        hashtree::MutableInterior* _root {nullptr};

        friend class HashTree::iterator;
    };
}
