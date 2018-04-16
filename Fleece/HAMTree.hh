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
        using bitmap_t = uint64_t;
        static constexpr int kBitShift = 6;
        static constexpr int kMaxChildren = 1 << kBitShift;
        static_assert(sizeof(bitmap_t) == kMaxChildren / 8, "Wrong constants");


        class Node {
        public:
            Node(uint8_t capacity)
            :_capacity(capacity)
            { }

            bool isLeaf() const {return _capacity == 0;}

        protected:
            uint8_t const _capacity;
        };


        class Target : public Node {
        public:
            Target(Key k)
            :Target(k.hash(), k)
            { }

            bool matches(hash_t h, Key k) const {
                return _hash == h && _key == k;
            }

            bool matches(const Target &target) {
                return matches(target._hash, target._key);
            }

            hash_t const _hash;
            Key const _key;

        protected:
            Target(hash_t h, Key k)
            :Node(0)
            ,_hash(h)
            ,_key(k)
            { }
        };


        class LeafNode : public Target {
        public:
            LeafNode(hash_t h, Key k, Val v)
            :Target(h, k)
            ,_val(v)
            { }

            LeafNode(Key k, Val v)
            :LeafNode(k.hash(), k, v)
            { }

            void dump(std::ostream&);

            Val _val;
        };


        class InteriorNode : public Node {
        public:
            InteriorNode()
            :InteriorNode(kMaxChildren, nullptr)
            { }

            InteriorNode* newInteriorNode(uint8_t capacity, InteriorNode *orig =nullptr);
            void freeChildren();

            uint8_t childCount() const;
            unsigned itemCount() const;
            LeafNode* find(hash_t) const;
            InteriorNode* insert(const LeafNode &target, unsigned shift);
            bool remove(const Target &target, unsigned shift);

            void dump(std::ostream &out, unsigned indent =1);

        private:
            static void* operator new(size_t, uint8_t capacity);

            InteriorNode(uint8_t capacity, InteriorNode* orig =nullptr)
            :Node(capacity)
            ,_bitmap(orig ? orig->_bitmap : 0)
            {
                if (orig)
                    memcpy(_children, orig->_children, orig->_capacity*sizeof(Node*));
                else
                    memset(_children, 0, _capacity*sizeof(Node*));
            }

            static unsigned childBitNumber(hash_t hash, unsigned shift =0)  {
                return (hash >> shift) & (kMaxChildren - 1);
            }

            int childIndexForBitNumber(unsigned bitNumber) const;

            bool hasChild(int i) const  {
                return ((_bitmap & (bitmap_t(1) << i)) != 0);
            }

            Node*& childForBitNumber(int bitNo);
            Node* const& childForBitNumber(int bitNo) const {
                return const_cast<InteriorNode*>(this)->childForBitNumber(bitNo);
            }
            InteriorNode* addChild(int bitNo, int childIndex, Node *child);
            InteriorNode* addChild(int bitNo, Node *child) {
                return addChild(bitNo, childIndexForBitNumber(bitNo), child);
            }
            InteriorNode* _addChildForBitNumber(int bitNo, int childIndex, Node *child);
            void removeChild(int bitNo, int childIndex);

            InteriorNode* grow();

            bitmap_t _bitmap {0};
            Node* _children[kMaxChildren];
        };

        InteriorNode _root;
    };

}
