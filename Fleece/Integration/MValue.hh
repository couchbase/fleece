//
// MValue.hh
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

#pragma once
#include "fleece/Fleece.hh"
#include "fleece/Base.h"
#include "fleece/slice.hh"

namespace fleece {
    using slice = fleece::slice;
    using alloc_slice = fleece::alloc_slice;

    template <class Native> class MCollection;


    /** Stores a Value together with its native equivalent.

        Can be changed to a different native value (which clears the original Value pointer.)
        The "Native" type is expected to be some type of smart pointer that holds a strong
        reference to a native object. (In Objective-C++, the type 'id' works because ARC ensures
        that the object is retained/released as necessary.)

        You will have to implement three methods in any specialization of this class; they're
        in the 'protected:' section below. These deal with creating Native objects from Fleece
        Values; mapping a Native back to a MValue; and encoding a Native object. */
    template <class Native>
    class MValue {
    public:
        constexpr MValue()                    { }
        constexpr MValue(Native n)            :_native(n) { }
        MValue(Value v)                       :_value(v) { }

        static const MValue empty;

        MValue(const MValue &mv)    =default;

        MValue(MValue &&mv) noexcept
        :_value(mv._value)
        ,_native(mv._native)
        {
            if (mv._native) {
                mv.nativeChangeSlot(this);
                mv._native = nullptr;
            }
        }

        ~MValue() {
            if (_native)
                nativeChangeSlot(nullptr);
        }

        MValue& operator= (MValue &&mv) noexcept {
            _native = mv._native;
            _value = mv._value;
            mv.setNative(nullptr);
            return *this;
        }

        MValue& operator= (const MValue &mv) {
            setNative(mv._native);
            _value = mv._value;
            return *this;
        }

        MValue& operator= (Native n) {
            if (_usuallyTrue(_native != n)) {
                setNative(n);
                _value = nullptr;
            }
            return *this;
        }

        Value value() const         {return _value;}
        bool isEmpty() const        {return !_value && !_native;}
        bool isMutated() const      {return !_value;}
        bool hasNative() const      {return _native != nullptr;}

        Native asNative(const MCollection<Native> *parent) const {
            if (_native || !_value) {
                return _native;
            } else {
                bool cacheIt = false;
                Native n = toNative(const_cast<MValue*>(this),
                                    const_cast<MCollection<Native>*>(parent),
                                    cacheIt);
                if (cacheIt)
                    _native = n;
                return n;
            }
        }

        void encodeTo(Encoder &enc) const {
            assert(!isEmpty());
            if (_value)
                enc.writeValue(_value);
            else
                encodeNative(enc, _native);
        }

        void mutate() {
            assert(_native);
            _value = nullptr;
        }

    protected:
        // These methods must be specialized for each specific Native type:

        /** Must instantiate and return a Native object corresponding to `mv->value()`.
            @param mv  The mutable value to create the Native object for.
            @param parent  The owning collection, if any.
            @param cacheIt  Set this to `true` if the Native object should be cached as part of
                     the MValue and returned automatically on the next call. This can help
                     performance if instantiation is slow, and it's _required_ when the value is
                     an Array or Dict.
            @return  A new Native object corresponding to mv's current value. */
        static Native toNative(MValue *mv, MCollection<Native> *parent, bool &cacheIt);

        /** Must return the MCollection object corresponding to the Native object,
            or null if the object doesn't correspond to a collection. */
        static MCollection<Native>* collectionFromNative(Native);

        /** Must write the Native object to the encoder as a Fleece value. */
        static void encodeNative(Encoder&, Native);

    private:
        void nativeChangeSlot(MValue *newSlot) {
            MCollection<Native>* collection = collectionFromNative(_native);
            if (collection)
                collection->setSlot(newSlot, this);
        }

        void setNative(Native n) {
            if (_usuallyTrue(_native != n)) {
                if (_native)
                    nativeChangeSlot(nullptr);
                _native = n;
                if (_native)
                    nativeChangeSlot(this);
            }
        }

        Value  _value;                      // Fleece value; null if I'm new or modified
        mutable Native _native {nullptr};   // Cached or new/modified native value
    };


    template <class Native>
    const MValue<Native> MValue<Native>::empty;

}
