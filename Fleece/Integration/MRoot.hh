//
// MRoot.hh
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
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

        void encodeTo(Encoder &enc) const   {_slot.encodeTo(enc, MCollection::context()->sharedKeys());}
        alloc_slice encode() const          {Encoder enc; encodeTo(enc); return enc.finish();}

        alloc_slice encodeDelta() const {
            Encoder enc;
            enc.makeDelta(context()->data());
            encodeTo(enc);
            return enc.finish();
        }

    private:
        MRoot(const MRoot&) =delete;
        MRoot& operator= (const MRoot &) =delete;

        MValue<Native>  _slot;              // My contents: a holder for the actual root object
    };

}
