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
        explicit MutableValue(MutableCollection *md) :_which(IsValuePointer), _asValue(md->asValue()) { }
        explicit MutableValue(const Value *v)   {*this = v;}  // operator= does the work
        ~MutableValue();

        MutableValue(const MutableValue&) noexcept =default;

        MutableValue(MutableValue &&other) noexcept {
            memcpy(this, &other, sizeof(other));
            other._malloced = false;
        }

        MutableValue& operator= (const MutableValue&) noexcept =default;

        MutableValue& operator= (MutableValue &&other) noexcept {
            reset();
            memcpy(this, &other, sizeof(other));
            other._malloced = false;
            return *this;
        }

        explicit operator bool() const  {return _which == IsInline || _asValue != nullptr;}

        const Value* asValue() const;
        MutableCollection* asMutableCollection() const;

        MutableValue& operator= (const Value *v);

        void set(MutableCollection *c) {
            reset();
            _which = IsValuePointer;
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

        void setInline(internal::tags valueTag, int tiny) {
            reset();
            _which = IsInline;
            _asInline[0] = uint8_t((valueTag << 4) | tiny);
        }

        void setInline(internal::tags valueTag, int tiny, int byte1) {
            setInline(valueTag, tiny);
            _asInline[1] = (uint8_t)byte1;
        }

        void setValue(internal::tags valueTag, int tiny, slice bytes);

        template <class INT>
        void setInt(INT, bool isUnsigned);
        void _setStringOrData(internal::tags valueTag, slice);
        uint8_t* allocateValue(size_t);

        enum Which : uint8_t {
            IsInline, IsValuePointer
        };

        union {
            uint8_t             _asInline[sizeof(void*)];
            const Value*        _asValue {nullptr};
        };
        Which _which {IsValuePointer};
        bool _malloced {false};
    };

} }
