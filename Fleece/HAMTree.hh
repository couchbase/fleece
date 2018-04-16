//
//  HAMTree.hh
//  Fleece
//  Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "slice.hh"
#include "Value.hh"

namespace fleece {

    using Key = alloc_slice;
    using Val = int;

    class HAMTree {
    public:
        HAMTree() { }

        void insert(Key, Val);
        bool remove(Key);

        Val get(Key);

    private:
        using hash_t = uint32_t;
        using bitmap_t = uint32_t;
        static constexpr int kBitSlice = 5;
        static constexpr int kMaxChildren = 1 << kBitSlice;
        static_assert(sizeof(bitmap_t) == kMaxChildren / 8, "Wrong constants");

        struct Node {
            uint8_t const _capacity;
            Node(uint8_t capacity)
            :_capacity(capacity)
            { }

            bool isLeaf() const {return _capacity == 0;}
        };

        struct LeafNode : public Node {
            LeafNode(hash_t h, Key k, Val v)
            :Node(0)
            ,_hash(h)
            ,_key(k)
            ,_val(v)
            { }

            hash_t const _hash;
            Key const _key;
            Val _val;
        };

        struct InteriorNode : public Node {
            uint32_t _bitmap {0};
            Node* _children[kMaxChildren] {};

            InteriorNode()
            :Node(kMaxChildren)
            { }
            ~InteriorNode();
            LeafNode* find(hash_t);
            void insert(hash_t, unsigned shift, Key key, Val val);
            bool remove(hash_t hash, unsigned shift, Key key);

            static int childIndex(hash_t hash, unsigned shift =0)  {
                return (hash >> shift) & (kMaxChildren - 1);
            }

            bool hasChild(int i) const  {return ((_bitmap & (1 << i)) != 0);}

            Node* childAtIndex(int i) {return _children[i];}  //TODO: use popcount

            void setChildAtIndex(int i, Node *child) {
                _children[i] = child;
                _bitmap |= (1 << i);
            }

            void removeChildAtIndex(int i) {
                _children[i] = nullptr;
                _bitmap &= ~(1 << i);
            }
        };

        InteriorNode _root;
    };

}
