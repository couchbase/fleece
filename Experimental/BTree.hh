//
// BTree.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "Array.hh"
#include "RefCounted.hh"
#include "PlatformCompat.hh"
#include <iostream>


namespace fleece {
    class MutableBTree;

    namespace btree {
        class iteratorImpl;
        uint32_t find(const Array*, slice key);
    }

    /** The root of an immutable B-tree encoded alongside Fleece data. */
    class BTree {
    public:
        static BTree fromData(slice data);
        
        BTree(const Value* NONNULL v)                   :_root(v) { }

        const Value* get(slice key) const;

        unsigned count() const;

        void dump(std::ostream &out) const;


        class iterator {
        public:
            iterator(const MutableBTree&);
            iterator(const BTree*);
            iterator(iterator&&);
            ~iterator();
            slice key() const noexcept                      {return _key;}
            const Value* value() const noexcept             {return _value;}
            explicit operator bool() const noexcept         {return _value != nullptr;}
            iterator& operator ++();
        private:
            iterator(Array*);
            std::unique_ptr<btree::iteratorImpl> _impl;
            slice _key;
            const Value *_value;
        };

    private:
        BTree()                                             :_root(nullptr) { }

        RetainedConst<Value> _root;

        friend class MutableBTree;
    };


}
