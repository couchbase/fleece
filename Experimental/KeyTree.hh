//
// KeyTree.hh
//
// Copyright (c) 2016 Couchbase, Inc All rights reserved.
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

#include "slice.hh"
#include <vector>

namespace fleece {

    /** A very compact dictionary of strings (or arbitrary blobs) that bidirectionally maps each
        one to a small positive integer. Internally it's stored as a tree, so lookup time is
        O(log n). The total storage overhead (beyond the sizes of the strings themselves) is
        about 1.5n bytes, although this increases somewhat as the length of the strings or the
        total size of the dictionary increase. */
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
