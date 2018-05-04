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

        static MutableCollection* asMutable(const Value *v) {
            if (!isMutable(v))
                return nullptr;
            auto coll = (MutableCollection*)(size_t(v) & ~1);
            assert(coll->_header[0] = 0xFF);
            return coll;
        }

        // Coercing MutableCollection -> Value:

        const Value* asValue() const                    {return (const Value*)&_header[1];}

        // MutableCollection API:

        static MutableCollection* mutableCopy(const Value *v, tags ifType);

        tags tag() const                                {return tags(_header[1] >> 4);}

        bool isChanged() const                          {return _changed;}

    protected:
        MutableCollection(internal::tags tag)
        :_header{0xFF, uint8_t(tag << 4)}
        ,_changed(false)
        { }

        uint8_t _header[2];
        bool _changed {false};
    };


    /** A value stored in a MutableDict or MutableArray. */
    class MutableValue {
    public:
        MutableValue() { }
        explicit MutableValue(MutableCollection *md)    :_asValue(md->asValue()) { }
        MutableValue(Null);
        ~MutableValue();

        MutableValue(const MutableValue&) noexcept =default;
        MutableValue& operator= (const MutableValue&) noexcept =default;

        MutableValue(MutableValue &&other) noexcept {
            memcpy(this, &other, sizeof(other));
            other._malloced = false;
        }

        MutableValue& operator= (MutableValue &&other) noexcept {
            reset();
            memcpy(this, &other, sizeof(other));
            other._malloced = false;
            return *this;
        }

        explicit operator bool() const                  {return _inline || _asValue != nullptr;}

        const Value* asValue() const;
        MutableCollection* asMutableCollection() const;

        void set(MutableCollection *c) {
            reset();
            _inline = false;
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
            uint8_t             _asInline[sizeof(void*)];
            const Value*        _asValue {nullptr};
        };
        bool _inline {false};
        bool _malloced {false};
    };

} }
