//
//  MValue.hh
//  Fleece
//
//  Created by Jens Alfke on 5/27/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "FleeceCpp.hh"
#include "slice.hh"

namespace fleeceapi {
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
        :_native(mv._native)
        ,_value(mv._value)
        {
            if (mv._native) {
                mv.nativeChangeSlot(this);
                mv._native = nullptr;
            }
        }

#ifndef NDEBUG
        virtual
#endif
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
            if (_native != n) {
                setNative(n);
                _value = nullptr;
            }
            return *this;
        }

        Value  value() const  {return _value;}
        bool isEmpty() const        {return !_value && !_native;}
        bool isMutated() const      {return !_value;}

        Native asNative(const MCollection<Native> *parent) const {
            if (!_native && _value)
                return const_cast<MValue*>(this)->toNative(const_cast<MCollection<Native>*>(parent));
            return _native;
        }

        void encodeTo(Encoder &enc) const {
            if (_value)
                enc << _value;
            else
                encodeNative(enc, _native);
        }

        void mutate() {
            assert(_native);
            _value = nullptr;
        }

    protected:
        // These methods must be specialized for each specific Native type:

        /** Must instantiate and return a Native object corresponding to the receiver.
            May cache the object by assigning it to `_native`.
            @param parent  The owning collection, if any
            @param asMutable  True if the Native object should be a mutable collection.
            @return  A new Native object corresponding to the receiver. */
        Native toNative(MCollection<Native> *parent);

        /** Must return the MCollection object corresponding to _native,
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
            if (_native != n) {
                if (_native)
                    nativeChangeSlot(nullptr);
                _native = n;
                if (_native)
                    nativeChangeSlot(this);
            }
        }

        Value  _value  {nullptr};     // Fleece value; null if I'm new or modified
        Native       _native {nullptr};     // Cached or new/modified native value
    };


    template <class Native>
    const MValue<Native> MValue<Native>::empty;

}
