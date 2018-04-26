//
//  HashTree+Internal.hh
//  Fleece
//
//  Created by Jens Alfke on 4/25/18.
//  Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "slice.hh"
#include "Value.hh"
#include "Bitmap.hh"
#include <memory>

namespace fleece { namespace hashtree {

    /*
         Data format:

        Interior Node:                  Leaf Node:
            bitmap   [4-byte int]          key   [4-byte offset]
            children [4-byte offset]       value [4-byte offset, OR'ed with 1]
        Children:
            is a contiguous array of (8-byte) interior & leaf nodes

        All numbers are little-endian.
        All offsets are byte counts backwards from the start of the containing node.

        The root node is at the end of the data, so it starts 8 bytes before the end.
     */


    // Types for the hash-array map:
    using hash_t = uint32_t;
    using bitmap_t = uint32_t;
    static constexpr int kBitShift = 5;                      // must be log2(8*sizeof(bitmap_t))
    static constexpr int kMaxChildren = 1 << kBitShift;
    static_assert(sizeof(bitmap_t) == kMaxChildren / 8, "Wrong constants");


    // Little-endian 32-bit integer
    class endian {
    public:
        endian(uint32_t o) {
            o = _encLittle32(o);
            memcpy(bytes, &o, sizeof(bytes));
        }

        operator uint32_t () const {
            uint32_t o;
            memcpy(&o, bytes, sizeof(o));
            return _decLittle32(o);
        }
    private:
        uint8_t bytes[sizeof(uint32_t)];
    };


    union Node;
    class MInteriorNode;


    // Internal class representing a leaf node
    class Leaf {
    public:
        const Value* key() const;
        const Value* value() const;
        slice keyString() const;

        hash_t hash() const             {return keyString().hash();}

        bool matches(slice key) const   {return keyString() == key;}

        void dump(std::ostream&, unsigned indent) const;
    private:
        endian _keyOffset;
        endian _valueOffset;

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

        bitmap_t bitmap() const;

        void dump(std::ostream&, unsigned indent) const;

    private:
        endian _bitmap;
        endian _childrenOffset;
    };


    union Node {
        Leaf leaf;
        Interior interior;

        bool isLeaf() const                 {return (leaf._valueOffset & 1) != 0;}
    };
} }

