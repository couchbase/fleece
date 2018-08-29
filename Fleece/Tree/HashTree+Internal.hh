//
//  HashTree+Internal.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once
#include "fleece/slice.hh"
#include "Value.hh"
#include "Bitmap.hh"
#include "Encoder.hh"
#include "Endian.hh"
#include <memory>

namespace fleece { namespace impl { namespace hashtree {

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


    // Internal class representing a leaf node
    class Leaf {
    public:
        const Value* key() const;
        const Value* value() const;
        slice keyString() const;

        hash_t hash() const             {return keyString().hash();}

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
        uint32_le_unaligned _keyOffset;
        uint32_le_unaligned _valueOffset;

        friend union Node;
        friend class Interior;
        friend class MutableInterior;
    };


    // Internal class representing an interior node
    class Interior {
    public:
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
        uint32_le_unaligned _bitmap;
        uint32_le_unaligned _childrenOffset;
    };


    union Node {
        Leaf leaf;
        Interior interior;

        Node() { }
        bool isLeaf() const                 {return (leaf._valueOffset & 1) != 0;}
    };
    
} } }

