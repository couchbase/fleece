//
//  HashTree.hh
//  Fleece
//
//  Created by Jens Alfke on 4/23/18.
//Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "slice.hh"
#include "Value.hh"
#include <memory>

namespace fleece {

    namespace hashtree {
        using hash_t = uint32_t;
        using bitmap_t = uint32_t;
        static constexpr int kBitShift = 5;                      // must be log2(8*sizeof(bitmap_t))
        static constexpr int kMaxChildren = 1 << kBitShift;
        static_assert(sizeof(bitmap_t) == kMaxChildren / 8, "Wrong constants");

        using offset = uint32_t;

        union Node;


        // Interior class representing a leaf node
        class Leaf {
        public:
            const Value* key() const;
            const Value* value() const;
            slice keyString() const;

        private:
            offset _keyOffset;              // Little-endian
            offset _valueOffset;            // Little-endian; always ORed with 1 as a tag

            friend union Node;
        };


        // Internal class representing an interior node
        struct Interior {
        public:
            const Leaf* findNearest(hash_t hash) const;
            unsigned leafCount() const;

            unsigned childCount() const;
            const Node* childAtIndex(int i) const;
            bool hasChild(unsigned bitNo) const;
            const Node* childForBitNumber(unsigned bitNo) const;

            bitmap_t _bitmap;              // Little-endian
            offset _childrenOffset;        // Little-endian
        };
    }

    /** The root of an immutable tree encoded alongside Fleece data. */
    class HashTree {
    public:
        using Key = slice;

        static const HashTree* at(const void *address)         {return (const HashTree*)address;}

        const Value* get(Key) const;

        unsigned count() const;

        void dump(std::ostream &out);

    private:
        const hashtree::Interior* getRoot() const;

        uint32_t _rootOffset;
    };
}
