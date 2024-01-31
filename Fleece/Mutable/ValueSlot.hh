//
// ValueSlot.hh
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "HeapValue.hh"
#include "Endian.hh"
#include <limits.h>

namespace fleece { namespace impl {
    namespace internal {
        class HeapArray;
        class HeapDict;
    }


    /** A mutable element of a HeapDict or HeapArray. It can store a Value either as a pointer
        or as an inline copy (if it's small enough.) */
    class ValueSlot {
    public:
        ValueSlot();
        ValueSlot(Null);
        ~ValueSlot();
        ValueSlot(internal::HeapCollection *md);

        ValueSlot(const ValueSlot&) noexcept;
        ValueSlot& operator= (const ValueSlot&) noexcept;
        ValueSlot(ValueSlot &&other) noexcept;
        ValueSlot& operator= (ValueSlot &&other) noexcept;

        bool empty() const FLPURE                              {return _pointer == 0;}
        explicit operator bool() const FLPURE                  {return !empty();}

        const Value* asValue() const FLPURE     {return isPointer() ? pointer() : inlinePointer();}
        const Value* asValueOrUndefined() const FLPURE;

        // Setters for the various Value types:
        void set(Null);
        void set(bool);
        void set(int i);
        void set(unsigned i);
        void set(int64_t);
        void set(uint64_t);
        void set(float);
        void set(double);
        void set(slice s)           {setStringOrData(internal::kStringTag, s);}
        void setData(slice s)       {setStringOrData(internal::kBinaryTag, s);}
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

        /** If my value is a HeapArray or HeapDict, returns it; else returns nullptr. */
        internal::HeapCollection* asMutableCollection() const;

        /** Promotes Array or Dict value to mutable equivalent in place, and returns it. */
        internal::HeapCollection* makeMutable(internal::tags ifType);

    private:
        void set(float, int tiny);
        bool isPointer() const noexcept FLPURE          {return _tag != kInlineTag;}
        bool isInline() const noexcept FLPURE           {return !isPointer();}
        const Value* pointer() const noexcept FLPURE    {return (const Value*)_pointer;}
        const Value* asPointer() const noexcept FLPURE  {return (isPointer() ? pointer() : nullptr);}
        const Value* inlinePointer() const noexcept FLPURE {return (const Value*)&_inlineVal;}
        void setPointer(const Value*);
        void setInline(internal::tags valueTag, int tiny);

        void releaseValue();
        void setValue(internal::tags valueTag, int tiny, slice bytes);
        template <class INT> void setInt(INT);
        void setStringOrData(internal::tags valueTag, slice);

        // The data layout below looks weirder than it is! It's just a union of a pointer and
        // a byte array.
        // It can store either a pointer to a Value, or 7 bytes of inline Value data.
        // The least significant byte of _pointer is used as a tag: if 0xFF the object is storing
        // inline data, else it's a pointer.
        //
        // This works because any pointer stored by a ValueSlot will be a Fleece value, and
        // * Immutable Values (interior pointers in encoded data) are always even;
        // * Mutable Values are allocated by malloc, which means at least 4-byte alignment.
        //   The low bit is set to 1 as a tag to indicate it's mutable, but that still leaves at
        //   at least one zero bit above that.
        //
        // The #ifdefs are to ensure that the _tag byte lines up with the least significant
        // byte of _pointer, and _inlineVal doesn't.

        static const auto kInlineCapacity = 7;

        union {
            uint64_t _pointer;                          // Pointer representation

            struct {
#ifdef __LITTLE_ENDIAN__
                uint8_t _tag;                           // Tag, lines up with LSB of _pointer
#endif
                uint8_t _inlineVal[kInlineCapacity];    // Inline Value representation
#ifdef __BIG_ENDIAN__
                uint8_t _tag;
#endif
            };
        };

        static constexpr uint8_t kInlineTag = 0xFF;
    };

} }
