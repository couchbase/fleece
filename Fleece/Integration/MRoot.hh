//
// MRoot.hh
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "MCollection.hh"

namespace fleece {

    /** Top-level object; a type of special single-element Collection that contains the root. */
    template <class Native>
    class MRoot : private MCollection<Native> {
    public:
        using MCollection = MCollection<Native>;

        MRoot() =default;

        explicit MRoot(MContext *context,
                       Value value,
                       bool isMutable)
        :MCollection(context, isMutable)
        ,_slot(value)
        { }

        explicit MRoot(MContext *context,
                       bool isMutable =true)
        :MRoot(context, Value::fromData(context->data(), kFLUntrusted), isMutable)
        { }

        explicit MRoot(alloc_slice fleeceData,
                       Value value,
                       bool isMutable =true)
        :MRoot(new MContext(fleeceData), isMutable)
        { }

        explicit MRoot(alloc_slice fleeceData,
                       bool isMutable =true)
        :MRoot(fleeceData, Value::fromData(fleeceData, kFLUntrusted), isMutable)
        { }

        static Native asNative(alloc_slice fleeceData,
                               bool mutableContainers =true)
        {
            MRoot root(fleeceData, mutableContainers);
            return root.asNative();
        }

        explicit operator bool() const      {return !_slot.isEmpty();}

        MContext* context() const           {return MCollection::context();}

        Native asNative() const             {return _slot.asNative(this);}
        bool isMutated() const              {return _slot.isMutated();}

        void encodeTo(Encoder &enc) const   {_slot.encodeTo(enc);}
        alloc_slice encode() const          {Encoder enc; encodeTo(enc); return enc.finish();}

        alloc_slice amend(bool reuseStrings =false, bool externPointers =false) const {
            Encoder enc;
            enc.amend(context()->data(), reuseStrings, externPointers);
            encodeTo(enc);
            return enc.finish();
        }

    private:
        MRoot(const MRoot&) =delete;
        MRoot& operator= (const MRoot &) =delete;

        MValue<Native>  _slot;              // My contents: a holder for the actual root object
    };

}
