//
//  MutableHashTree.hh
//  Fleece
//  Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "HashTree.hh"
#include "Function.hh"
#include "fleece/slice.hh"
#include <memory>

namespace fleece {
    class MutableArray;
    class MutableDict;
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

        MutableHashTree& operator= (MutableHashTree&&) noexcept;
        MutableHashTree& operator= (const HashTree*);

        Value get(slice key) const;

        MutableArray getMutableArray(slice key);
        MutableDict getMutableDict(slice key);

        unsigned count() const;

        bool isChanged() const                  {return _root != nullptr;}

        using InsertCallback = Function<Value(Value)>;

        void set(slice key, Value);
        bool insert(slice key, InsertCallback);
        bool remove(slice key);

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
