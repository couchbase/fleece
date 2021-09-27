//
//  HashTree+Internal.hh
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "fleece/slice.hh"
#include "fleece/Fleece.hh"
#include "Bitmap.hh"
#include "Endian.hh"
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


    union Node;
    class MutableInterior;

    // Types for the hash-array map:
    using hash_t = uint32_t;
    using bitmap_t = uint32_t;
    static constexpr int kBitShift = 5;                      // must be log2(8*sizeof(bitmap_t))
    static constexpr int kMaxChildren = 1 << kBitShift;
    static_assert(sizeof(bitmap_t) == kMaxChildren / 8, "Wrong constants");

    // Hashes a key. The hash value for a key must always be the same no matter what platform or
    // software version this is, since the structure of the hash table depends on it.
    FLPURE hash_t ComputeHash(slice key) noexcept;

    // Internal class representing a leaf node
    class Leaf {
    public:
        void validate() const;

        Value key() const;
        Value value() const;
        slice keyString() const;

        hash_t hash() const             {return ComputeHash(keyString());}

        bool matches(slice key) const   {return keyString() == key;}

        void dump(std::ostream&, unsigned indent) const;

        uint32_t keyOffset() const             {return _keyOffset;}
        uint32_t valueOffset() const           {return _keyOffset;}

        Leaf(uint32_t keyPos, uint32_t valuePos)
        :_keyOffset(keyPos)
        ,_valueOffset(valuePos)
        { }

        void makeRelativeTo(uint32_t pos) {
            _keyOffset = pos - _keyOffset;
            _valueOffset = (pos - _valueOffset) | 1;
        }

        Leaf makeAbsolute(uint32_t pos) const {
            return Leaf(pos - _keyOffset, pos - (_valueOffset & ~1));
        }

        uint32_t writeTo(Encoder&, bool writeKey) const;

    private:
        endian::uint32_le_unaligned _keyOffset;
        endian::uint32_le_unaligned _valueOffset;

        friend union Node;
        friend class Interior;
        friend class MutableInterior;
    };


    // Internal class representing an interior node
    class Interior {
    public:
        void validate() const;

        const Leaf* findNearest(hash_t hash) const;
        unsigned leafCount() const;

        unsigned childCount() const;
        const Node* childAtIndex(int i) const;
        bool hasChild(unsigned bitNo) const;
        const Node* childForBitNumber(unsigned bitNo) const;

        bitmap_t bitmap() const;

        void dump(std::ostream&, unsigned indent) const;

        uint32_t childrenOffset() const             {return _childrenOffset;}

        Interior(bitmap_t bitmap, uint32_t childrenPos)
        :_bitmap(bitmap)
        ,_childrenOffset(childrenPos)
        { }
        
        void makeRelativeTo(uint32_t pos) {
            _childrenOffset = pos - _childrenOffset;
        }

        Interior makeAbsolute(uint32_t pos) const {
            return Interior(_bitmap, pos - _childrenOffset);
        }

        Interior writeTo(Encoder&) const;

    private:
        endian::uint32_le_unaligned _bitmap;
        endian::uint32_le_unaligned _childrenOffset;
    };


    union Node {
        Leaf leaf;
        Interior interior;

        Node() { }
        bool isLeaf() const                 {return (leaf._valueOffset & 1) != 0;}

        const Node* validate() const {
            if (isLeaf()) leaf.validate(); else interior.validate();
            return this;
        }
    };
    
} }

