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


    /** A value stored in a MutableDict or MutableArray. */
    class MutableValue {
    public:
        MutableValue() { }
        explicit MutableValue(MutableCollection *md) :_which(IsMutableCollection), _asCollection(md) { }
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

        const Value* asValuePointer() const;
        MutableCollection* asMutableCollection() const;

        MutableValue& operator= (const Value *v);

        void set(MutableCollection *c) {
            reset();
            _which = IsMutableCollection;
            _asCollection = c;
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
        const Value* makeMutable(tags ifType);

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
            IsInline, IsValuePointer, IsMutableCollection
        };

        union {
            uint8_t             _asInline[sizeof(void*)];
            const Value*        _asValue {nullptr};
            MutableCollection*  _asCollection;;
        };
        Which _which {IsValuePointer};
        bool _malloced {false};
    };


    /** Abstract base class of Mutable{Array,Dict}. */
    class MutableCollection {
    public:
        const Value* asValuePointer() const              {return (const Value*)&_header[1];}

        static bool isMutableCollection(const Value *v)  {return ((size_t)v & 1) != 0;}

        static MutableCollection* asMutableCollection(const Value *v) {
            if (!isMutableCollection(v))
                return nullptr;
            auto coll = (MutableCollection*)(size_t(v) & ~1);
            assert(coll->_header[1] = 0xFF);
            return coll;
        }

        static MutableArray* asMutableArray(const Value *v) {
            return (MutableArray*)asMutableCollection(v, internal::kArrayTag << 4);
        }

        static MutableDict* asMutableDict(const Value *v) {
            return (MutableDict*)asMutableCollection(v, internal::kDictTag << 4);
        }

        tags tag() const                                {return tags(_header[0] >> 4);}

        bool isChanged() const                          {return _changed;}

    protected:
        MutableCollection(internal::tags tag)
        :_header{uint8_t(tag << 4), 0xFF}
        ,_changed(false)
        { }

        static MutableCollection* asMutableCollection(const Value *v, int tagByte) {
            auto coll = asMutableCollection(v);
            return (coll && coll->_header[0] == tagByte) ? coll : nullptr;
        }

        uint8_t _header[2];
        bool _changed {false};
    };

} }
