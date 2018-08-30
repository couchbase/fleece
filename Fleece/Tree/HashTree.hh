//
//  HashTree.hh
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
#include "fleece/Fleece.hh"

namespace fleece {

    class MutableHashTree;

    namespace hashtree {
        class Interior;
        class MutableInterior;
        class NodeRef;
        struct iteratorImpl;
    }


    /** The root of an immutable tree encoded alongside Fleece data. */
    class HashTree {
    public:
        static const HashTree* fromData(slice data);

        Value get(slice) const;

        unsigned count() const;

        void dump(std::ostream &out) const;


        class iterator {
        public:
            iterator(const MutableHashTree&);
            iterator(const HashTree*);
            iterator(iterator&&);
            ~iterator();
            slice key() const noexcept                      {return _key;}
            Value value() const noexcept                    {return _value;}
            explicit operator bool() const noexcept         {return !!_value;}
            iterator& operator ++();
        private:
            iterator(hashtree::NodeRef);
            std::unique_ptr<hashtree::iteratorImpl> _impl;
            slice _key;
            Value _value;
        };

    private:
        const hashtree::Interior* rootNode() const;

        friend class hashtree::MutableInterior;
        friend class MutableHashTree;
    };
}
