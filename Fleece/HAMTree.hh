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
        HAMTree()
        :_root()
        { }

        ~HAMTree() {
            _root.freeChildren();
        }

        void insert(Key, Val);
        bool remove(Key);

        Val get(Key);

        unsigned count() {
            return _root.itemCount();
        }

        void dump(std::ostream &out);

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

            void dump(std::ostream&);

            hash_t const _hash;
            Key const _key;
            Val _val;
        };

        struct InteriorNode : public Node {
            uint32_t _bitmap {0};
            Node* _children[kMaxChildren];

            InteriorNode()
            :InteriorNode(kMaxChildren)
            { }

            InteriorNode* newInteriorNode(uint8_t capacity);
            void freeChildren();

            uint8_t childCount();
            unsigned itemCount();
            LeafNode* find(hash_t);
            InteriorNode* insert(hash_t, unsigned shift, Key key, Val val);
            bool remove(hash_t hash, unsigned shift, Key key);

            void dump(std::ostream &out, unsigned indent =1);

        private:
            InteriorNode(uint8_t capacity)
            :Node(capacity)
            {
                memset(_children, 0, _capacity * sizeof(Node*));
            }

            void* operator new(size_t, uint8_t capacity);

            static unsigned childBitNumber(hash_t hash, unsigned shift =0)  {
                return (hash >> shift) & (kMaxChildren - 1);
            }

            int childIndexForBitNumber(unsigned bitNumber);

            bool hasChild(int i) const  {
                return ((_bitmap & (1 << i)) != 0);
            }

            Node*& childForBitNumber(int i);
            InteriorNode* addChildForBitNumber(int i, Node *child);
            InteriorNode* _addChildForBitNumber(int bitNo, Node *child);
            void replaceChildForBitNumber(int i, Node *child);
            void removeChildForBitNumber(int i);

            InteriorNode* grow();
        };

        InteriorNode _root;
    };

}
