//
//  MValue.hh
//  Fleece
//
//  Created by Jens Alfke on 5/27/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "Fleece.hh"

namespace fleece {

    template <class Native> class MCollection;


    /** Stores a Value together with its native equivalent.
        Can be changed to a different native value (which clears the original Value pointer.) */
    template <class Native>
    class MValue {
    public:
        constexpr MValue()                    { }
        constexpr MValue(Native n)            :_native(n) { }
        constexpr MValue(const Value *v)      :_value(v) { }

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

        ~MValue() {
            setNative(nullptr);
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

        bool isEmpty() const        {return _value == nullptr && _native == nullptr;}
        bool isMutated() const      {return _value == nullptr;}

        Native asNative(const MCollection<id> *parent, bool asMutable) const {
            if (!_native && _value)
                return const_cast<MValue*>(this)->toNative(const_cast<MCollection<id>*>(parent),
                                                           asMutable);
            return _native;
        }

        const Value* value() const {
            return _value;
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

        void nativeChangeSlot(MValue *newSlot) {
            MCollection<Native>* val = fromNative(_native);
            if (val)
                val->setSlot(newSlot, this);
        }

    protected:
        // These methods must be specialized for each specific Native type:

        /** Must instantiate and return a Native object corresponding to the receiver.
             May cache the object by assigning it to `_native`.
             @param parent  The owning collection, if any
             @param asMutable  True if the Native object should be a mutable collection.
             @return  A new Native object corresponding to the receiver. */
        Native toNative(MCollection<id> *parent, bool asMutable);

        /** Must return the MCollection object corresponding to _native, if any. */
        static MCollection<Native>* fromNative(Native);

        /** Must write the Native object to the encoder as a Fleece value. */
        static void encodeNative(Encoder&, Native);

    private:
        void setNative(Native n) {
            if (_native != n) {
                if (_native)
                    nativeChangeSlot(nullptr);
                _native = n;
            }
        }

        const Value* _value  {nullptr};     // Fleece value; null if I'm new or modified
        Native       _native {nullptr};     // Cached or new/modified native value
    };


    template <class Native>
    const MValue<Native> MValue<Native>::empty;

}
