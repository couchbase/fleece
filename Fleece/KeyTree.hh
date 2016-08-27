//
//  KeyTree.hh
//  Fleece
//
//  Created by Jens Alfke on 5/14/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//

#pragma once

#include "slice.hh"
#include <vector>

namespace fleece {

    /** A very compact dictionary of strings (or arbitrary blobs) that bidirectionally maps each
        one to a small positive integer. Internally it's stored as a tree, so lookup time is
        O(log n). The total storage overhead (beyond the sizes of the strings themselves) is
        about 1.5n bytes.
        NOTE: The current implementation is very limited in scalability. It can't handle more than
        about 512 bytes of total string data, and each string can't be longer than 255 bytes. */
    class KeyTree {
    public:
        KeyTree(const void *encodedDataStart);
        KeyTree(alloc_slice encodedData);
        
        static KeyTree fromSortedStrings(const std::vector<slice>&);
        static KeyTree fromStrings(std::vector<slice>);

        unsigned operator[] (slice str) const;
        slice operator[] (unsigned id) const;

        slice encodedData() const       {return _ownedData;}

    private:
        alloc_slice _ownedData;
        const void * _data;
    };

}
