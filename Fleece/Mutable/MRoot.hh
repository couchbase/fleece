//
//  MRoot.hh
//  Fleece
//
//  Created by Jens Alfke on 10/5/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "MCollection.hh"

namespace fleeceapi {

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
        :MRoot(context, Value::fromData(context->data()), isMutable)
        { }

        explicit MRoot(alloc_slice fleeceData,
                       FLSharedKeys sk,
                       Value value,
                       bool isMutable =true)
        :MRoot(new MContext(fleeceData, sk), isMutable)
        { }

        explicit MRoot(alloc_slice fleeceData,
                       FLSharedKeys sk =nullptr,
                       bool isMutable =true)
        :MRoot(fleeceData, sk, Value::fromData(fleeceData), isMutable)
        { }

        static Native asNative(alloc_slice fleeceData,
                               FLSharedKeys sk =nullptr,
                               bool mutableContainers =true)
        {
            MRoot root(fleeceData, sk, mutableContainers);
            return root.asNative();
        }

        explicit operator bool() const      {return !_slot.isEmpty();}

        MContext* context() const           {return MCollection::context();}

        Native asNative() const             {return _slot.asNative(this);}
        bool isMutated() const              {return _slot.isMutated();}

        void encodeTo(Encoder &enc) const   {_slot.encodeTo(enc);}
        alloc_slice encode() const          {Encoder enc; encodeTo(enc); return enc.finish();}

        alloc_slice encodeDelta() const {
            Encoder enc;
            enc.makeDelta(context()->data());
            encodeTo(enc);
            return enc.finish();
        }

    private:
        MValue<Native>  _slot;              // My contents: a holder for the actual root object
    };

}
