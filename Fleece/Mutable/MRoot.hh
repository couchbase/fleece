//
//  MRoot.hh
//  Fleece
//
//  Created by Jens Alfke on 10/5/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "MCollection.hh"

namespace fleece {

    /** Top-level object; a type of special single-element Collection that contains the root. */
    template <class Native>
    class MRoot : private MCollection<Native> {
    public:
        using MCollection = MCollection<Native>;

        MRoot() =default;

        MRoot(alloc_slice fleeceData,
              SharedKeys *sk,
              const Value *value,
              bool mutableContainers =true)
        :MCollection(new internal::Context(fleeceData, sk, mutableContainers))
        ,_slot(value)
        { }

        MRoot(alloc_slice fleeceData,
              SharedKeys *sk =nullptr,
              bool mutableContainers =true)
        :MRoot(fleeceData, sk, Value::fromData(fleeceData), mutableContainers)
        { }

        static Native asNative(alloc_slice fleeceData,
                               SharedKeys *sk =nullptr,
                               bool mutableContainers =true)
        {
            MRoot root(fleeceData, sk, mutableContainers);
            return root.asNative();
        }

        explicit operator bool() const      {return !_slot.isEmpty();}

        alloc_slice originalData() const    {return MCollection::originalData();}
        SharedKeys* sharedKeys() const      {return MCollection::sharedKeys();}

        Native asNative() const             {return _slot.asNative(this);}
        bool isMutated() const              {return _slot.isMutated();}
        void encodeTo(Encoder &enc) const   {_slot.encodeTo(enc);}

        alloc_slice encode() const          {Encoder enc; encodeTo(enc); return enc.extractOutput();}

        alloc_slice encodeDelta() const {
            Encoder enc;
            enc.setBase(originalData());
            enc.reuseBaseStrings();
            encodeTo(enc);
            return enc.extractOutput();
        }

    private:
        MValue<Native>  _slot;              // My contents: a holder for the actual root object
    };

}
