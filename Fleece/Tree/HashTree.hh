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

namespace fleece {

    namespace hashtree {
        class Interior;
        class MInteriorNode;
    }


    /** The root of an immutable tree encoded alongside Fleece data. */
    class HashTree {
    public:
        static const HashTree* fromData(slice data);

        const Value* get(slice) const;

        unsigned count() const;

        void dump(std::ostream &out) const;

    private:
        const hashtree::Interior* getRoot() const;

        friend class hashtree::MInteriorNode;
    };
}
