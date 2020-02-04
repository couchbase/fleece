//
// ValueSlot.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "HeapValue.hh"

namespace fleece { namespace impl {
    namespace internal {
        class HeapArray;
        class HeapDict;
    }


    /** A mutable element of a HeapDict or HeapArray. It can store a Value either as a pointer
        or as an inline copy (if it's small enough.) */
    class ValueSlot {
    public:
        ValueSlot() { }
        ValueSlot(Null);
        ~ValueSlot();
        ValueSlot(internal::HeapCollection *md);

        ValueSlot(const ValueSlot&) noexcept;
        ValueSlot& operator= (const ValueSlot&) noexcept;
        ValueSlot(ValueSlot &&other) noexcept;
        ValueSlot& operator= (ValueSlot &&other) noexcept;

        bool empty() const PURE                              {return !_isInline && _asValue == nullptr;}
        explicit operator bool() const PURE                  {return !empty();}

        const Value* asValue() const PURE;
        const Value* asValueOrUndefined() const PURE;

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

        /** Replaces an external value with a copy of itself. */
        void copyValue(CopyFlags);

    protected:
        friend class internal::HeapArray;
        friend class internal::HeapDict;

        internal::HeapCollection* asMutableCollection() const;

        /** Promotes Array or Dict value to mutable equivalent and returns it. */
        internal::HeapCollection* makeMutable(internal::tags ifType);

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

} }
