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


    namespace internal {
        class MutableValue : public Value {
        public:
            MutableValue()
            :Value(internal::kSpecialTag, internal::kSpecialValueNull) { }

            void set(Null);
            void set(bool);
            void set(int i)             {set((int64_t)i);}
            void set(unsigned i)        {set((uint64_t)i);}
            void set(int64_t);
            void set(uint64_t);
            void set(slice s)           {set(internal::kStringTag, s);}
            void set(const Value* v)    {_set(v); _changed = true;}

            void copy(const Value*);

            MutableArray* makeArrayMutable();
            MutableDict* makeDictMutable();

            bool isChanged() const      {return _changed;}
            void setChanged(bool c)     {_changed = c;}

            static const Value* derefPointer(const MutableValue *v) {
                return (Value*)( _dec64(*(size_t*)v) << 1 );
            }

            const Value* deref() const {
                return isPointer() ? derefPointer(this) : this;
            }

        private:
            void setHeader(internal::tags tag, int tiny) {
                _byte[0] = (uint8_t)((tag<<4) | tiny);
                _changed = true;
            }
            void setHeader(internal::tags tag, int tiny, int byte1) {
                _byte[1] = (uint8_t)byte1;
                setHeader(tag, tiny);
            }

            void set(uint64_t i, bool isSmall, bool isUnsigned);
            void set(internal::tags, slice s);
            void _set(const Value*);

            static constexpr size_t kMaxInlineValueSize = 10; // Enough room to hold ints & doubles

            uint8_t _moreBytes[kMaxInlineValueSize - sizeof(_byte)];
            bool _changed {false};
            bool _unused[5];        // Bring total size up to 16 bytes
        };
    }

}
