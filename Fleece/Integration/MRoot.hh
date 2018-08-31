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

        alloc_slice amend() const {
            Encoder enc;
            enc.amend(context()->data());
            encodeTo(enc);
            return enc.finish();
        }

    private:
        MRoot(const MRoot&) =delete;
        MRoot& operator= (const MRoot &) =delete;

        MValue<Native>  _slot;              // My contents: a holder for the actual root object
    };

}
