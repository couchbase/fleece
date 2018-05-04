//
// MutableValue.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "Value.hh"

namespace fleece {
    class MutableArray;
    class MutableDict;
}

namespace fleece { namespace internal {
    class MutableCollection;


    /** Abstract base class of Mutable{Array,Dict}. */
    class MutableCollection {
    public:

        // Coercing Value --> MutableCollection:

        static bool isMutable(const Value *v)           {return ((size_t)v & 1) != 0;}
        static MutableCollection* asMutable(const Value *v);
        static MutableCollection* mutableCopy(const Value *v, tags ifType);

        // Coercing MutableCollection -> Value:

        const Value* asValue() const                    {return (const Value*)&_header[1];}

        // MutableCollection API:

        tags tag() const                                {return tags(_header[1] >> 4);}
        bool isChanged() const                          {return _changed;}

    protected:
        MutableCollection(internal::tags tag)
        :_header{0xFF, uint8_t(tag << 4)}
        ,_changed(false)
        { }

        void setChanged(bool c)                         {_changed = c;}

    private:
        uint8_t _header[2];             // *2nd* byte is a Value header byte
        bool _changed {false};
    };


    /** A value stored in a MutableDict or MutableArray. */
    class MutableValue {
    public:
        MutableValue() { }
        explicit MutableValue(MutableCollection *md)    :_asValue(md->asValue()) { }
        MutableValue(Null);
        ~MutableValue();

        MutableValue(const MutableValue&) noexcept;
        MutableValue& operator= (const MutableValue&) noexcept;

        MutableValue(MutableValue &&other) noexcept {
            memcpy(this, &other, sizeof(other));
            other._isMalloced = false;
        }

        MutableValue& operator= (MutableValue &&other) noexcept {
            reset();
            memcpy(this, &other, sizeof(other));
            other._isMalloced = false;
            return *this;
        }

        explicit operator bool() const                  {return _isInline || _asValue != nullptr;}

        const Value* asValue() const;
        MutableCollection* asMutableCollection() const;

        void set(MutableCollection *c) {
            reset();
            _isInline = false;
            _asValue = c->asValue();
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
        void reset();
        void setInline(internal::tags valueTag, int tiny);
        void setValue(internal::tags valueTag, int tiny, slice bytes);
        template <class INT> void setInt(INT, bool isUnsigned);
        void _setStringOrData(internal::tags valueTag, slice);
        uint8_t* allocateValue(size_t);

        union {
            uint8_t             _inlineData[sizeof(void*)];
            const Value*        _asValue {nullptr};
        };
        uint8_t _moreInlineData[sizeof(void*) - 2];
        bool _isInline {false};
        bool _isMalloced {false};

        static constexpr size_t kInlineCapacity = sizeof(_inlineData) + sizeof(_moreInlineData);
    };

} }
