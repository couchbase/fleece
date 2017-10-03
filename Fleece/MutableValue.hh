//
//  MutableValue.hh
//  Fleece
//
//  Created by Jens Alfke on 9/21/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "Value.hh"

namespace fleece {
    class MutableArray;
    class MutableDict;


    // A Value that can be changed. Used by MutableArray and MutableDict.
    namespace internal {
        class MutableValue : public Value {
        public:
            MutableValue()
            :Value(internal::kSpecialTag, internal::kSpecialValueNull)
            ,_exists(false)
            ,_changed(false)
            ,_malloced(false)
            { }

            MutableValue(const MutableValue&);

            ~MutableValue()             {reset();}

            // Setters for the various Value types:
            void set(Null);
            void set(bool);
            void set(int i)             {set((int64_t)i);}
            void set(unsigned i)        {set((uint64_t)i);}
            void set(int64_t);
            void set(uint64_t);
            void set(slice s)           {_set(internal::kStringTag, s);}
            void setData(slice s)       {_set(internal::kBinaryTag, s);}
            void set(const Value* v);

            /** Promotes Array or Dict value to mutable equivalent and returns it. */
            const Value* makeMutable(valueType ifType);

            bool exists() const         {return _exists;}
            void setNonexistent()       {set(nullValue); _exists = false;}
            
            bool isChanged() const      {return _changed;}
            void setChanged(bool c)     {_changed = c;}

            const Value* deref() const  {return isPointer() ? derefPointer(this) : this;}

            void copy(const Value*); // internal only (has to be public for dumb C++ reasons)

        private:
            void reset();

            void setHeader(internal::tags tag, int tiny) {
                reset();
                _byte[0] = (uint8_t)((tag<<4) | tiny);
                _changed = _exists = true;
            }
            void setHeader(internal::tags tag, int tiny, int byte1) {
                _byte[1] = (uint8_t)byte1;
                setHeader(tag, tiny);
            }

            static const Value* derefPointer(const MutableValue *v) {
                return (Value*)( _dec64(*(size_t*)v) << 1 );    // Same as regular ptr, just 64-bit
            }

            void set(uint64_t i, bool isSmall, bool isUnsigned);
            void _set(internal::tags, slice s);
            void _setPointer(const Value*);

            static constexpr size_t kMaxInlineValueSize = 10; // Enough room to hold ints & doubles

            uint8_t _moreBytes[kMaxInlineValueSize - sizeof(_byte)];
            bool _exists   :1;      // If false, there's nothing stored here
            bool _changed  :1;      // Means value has changed since I was created
            bool _malloced :1;      // Means Value I point to is a malloced block owned by me
            bool _unused[5];        // Just brings total size up to 16 bytes

            friend class fleece::MutableDict;
        };
    }

}
