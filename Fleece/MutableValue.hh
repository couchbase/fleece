//
// MutableValue.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "HeapValue.hh"

namespace fleece {
    class MutableArray;
    class MutableDict;
}

namespace fleece { namespace internal {
    class MutableCollection;


    /** Abstract base class of Mutable{Array,Dict}. */
    class MutableCollection : public HeapValue {
    public:

        // Coercing Value --> MutableCollection:

        static bool isMutable(const Value *v)           {return isHeapValue(v);}
        static MutableCollection* mutableCopy(const Value *v, tags ifType);

        // MutableCollection API:

        bool isChanged() const                          {return _changed;}

    protected:
        MutableCollection(internal::tags tag)
        :HeapValue(tag, 0)
        ,_changed(false)
        { }
        ~MutableCollection() =default;

        void setChanged(bool c)                         {_changed = c;}

    private:
        bool _changed {false};
    };


    /** A value stored in a MutableDict or MutableArray. */
    class MutableValue {
    public:
        MutableValue() { }
        explicit MutableValue(MutableCollection *md);
        MutableValue(Null);
        ~MutableValue();

        MutableValue(const MutableValue&) noexcept;
        MutableValue& operator= (const MutableValue&) noexcept;
        MutableValue(MutableValue &&other) noexcept;
        MutableValue& operator= (MutableValue &&other) noexcept;

        explicit operator bool() const                  {return _isInline || _asValue != nullptr;}

        const Value* asValue() const;
        MutableCollection* asMutableCollection() const;

        void set(MutableCollection *c) {
            auto newValue = c->asValue();
            if (!_isInline) {
                if (newValue == _asValue)
                    return;
                releaseValue();
                _isInline = false;
            }
            _asValue = retain(c->asValue());
        }

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
        void set(const Value* v);

        /** Promotes Array or Dict value to mutable equivalent and returns it. */
        MutableCollection* makeMutable(tags ifType);

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

        static constexpr size_t kInlineCapacity = sizeof(_inlineData) + sizeof(_moreInlineData);
    };

} }
