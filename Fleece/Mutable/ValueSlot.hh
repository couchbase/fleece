//
// ValueSlot.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "HeapValue.hh"

namespace fleece { namespace impl { namespace internal {
    using namespace fleece::impl;
    class HeapCollection;


    /** A mutable element of a HeapDict or HeapArray. It can store a Value either as a pointer
        or as an inline copy (if it's small enough.) */
    class ValueSlot {
    public:
        ValueSlot() { }
        ValueSlot(HeapCollection *md);
        ValueSlot(Null);
        ~ValueSlot();

        ValueSlot(const ValueSlot&) noexcept;
        ValueSlot& operator= (const ValueSlot&) noexcept;
        ValueSlot(ValueSlot &&other) noexcept;
        ValueSlot& operator= (ValueSlot &&other) noexcept;

        explicit operator bool() const                  {return _isInline || _asValue != nullptr;}

        const Value* asValue() const;
        const Value* asValueOrUndefined() const;
        HeapCollection* asMutableCollection() const;

        // Setters for the various Value types:
        void set(Null);
        void set(bool);
        void set(int i);
        void set(unsigned i);
        void set(int64_t);
        void set(uint64_t);
        void set(float);
        void set(double);
        void set(slice s)           {_setStringOrData(internal::kStringTag, s);}
        void setData(slice s)       {_setStringOrData(internal::kBinaryTag, s);}
        void setValue(const Value* v);

        // These methods allow set(Value*) to be called, without allowing any other pointer type
        // to be used; without them, set(Foo*) would incorrectly call set(bool).
        template <class T> void set(const T* t)                 {setValue(t);}
        template <class T> void set(const Retained<T> &t)       {setValue(t);}
        template <class T> void set(const RetainedConst<T> &t)  {setValue(t);}

        /** Promotes Array or Dict value to mutable equivalent and returns it. */
        HeapCollection* makeMutable(tags ifType);

        /** Replaces an external value with a copy of itself. */
        void copyValue(CopyFlags);

    private:
        void releaseValue();
        void setInline(internal::tags valueTag, int tiny);
        void setValue(internal::tags valueTag, int tiny, slice bytes);
        template <class INT> void setInt(INT, bool isUnsigned);
        void _setStringOrData(internal::tags valueTag, slice);

        union {
            uint8_t             _inlineData[sizeof(void*)];
            const Value*        _asValue {nullptr};
        };

        uint8_t _moreInlineData[sizeof(void*) - 1];
        bool _isInline {false};

        static constexpr size_t kInlineCapacity = sizeof(ValueSlot::_inlineData) + sizeof(ValueSlot::_moreInlineData);
    };

} } }
