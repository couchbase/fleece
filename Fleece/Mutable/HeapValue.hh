//
// HeapValue.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "Value.hh"
#include "RefCounted.hh"

namespace fleece { namespace impl {

    namespace internal {
        using namespace fleece::impl;

        struct offsetValue {
            uint8_t _pad = 0xFF;                // Unused byte, to ensure _header is at an odd address
            uint8_t _header;                    // Value header byte (tag | tiny)
//          uint8_t _data[0];                   // Extra Value data (object is dynamically sized)

            offsetValue() {}
        private:
            offsetValue(const offsetValue&) = delete;
            offsetValue(offsetValue&&) = delete;
            offsetValue& operator=(const offsetValue&) = delete;
        };

        /** Stores a Value in a heap block.
            The actual Value data is offset by 1 byte, so that pointers to it are tagged. */
        class HeapValue : public RefCounted, offsetValue {
        public:
            static HeapValue* create(Null);
            static HeapValue* create(bool);
            static HeapValue* create(int i)             {return createInt(i, false);}
            static HeapValue* create(unsigned i)        {return createInt(i, true);}
            static HeapValue* create(int64_t i)         {return createInt(i, false);}
            static HeapValue* create(uint64_t i)        {return createInt(i, true);}
            static HeapValue* create(float);
            static HeapValue* create(double);
            static HeapValue* create(slice s)           {return createStr(internal::kStringTag, s);}
            static HeapValue* createData(slice s)       {return createStr(internal::kBinaryTag, s);}
            static HeapValue* create(const Value*);

            static const Value* asValue(HeapValue *v)   {return v ? v->asValue() : nullptr;}
            const Value* asValue() const                {return (const Value*)&_header;}

            static bool isHeapValue(const Value *v)     {return ((size_t)v & 1) != 0;}
            static HeapValue* asHeapValue(const Value*);

            static void retain(const Value *v);
            static void release(const Value *v);

            void* operator new(size_t size)                 {return ::operator new(size);}
            void operator delete(void* ptr)                 {::operator delete(ptr);}
            void operator delete(void* ptr, size_t size)    {::operator delete(ptr);}
        protected:
            ~HeapValue() =default;
            static HeapValue* create(tags tag, int tiny, slice extraData);
            HeapValue(tags tag, int tiny);
            tags tag() const                            {return tags(_header >> 4);}
        private:
            friend class ValueSlot;

            static void* operator new(size_t size, size_t extraSize);
            HeapValue() { }
            static HeapValue* createStr(internal::tags, slice s);
            template <class INT> static HeapValue* createInt(INT, bool isUnsigned);
        };



        /** Abstract base class of Heap{Array,Dict}. */
        class HeapCollection : public HeapValue {
        public:
            static Retained<HeapCollection> mutableCopy(const Value *v, tags ifType);

            bool isChanged() const                          {return _changed;}

        protected:
            HeapCollection(internal::tags tag)
            :HeapValue(tag, 0)
            ,_changed(false)
            { }

            ~HeapCollection() =default;

            void setChanged(bool c)                         {_changed = c;}

        private:
            bool _changed {false};
        };

    } // end internal namespace

    
    static inline const Value* retain(const Value *v) {
        if (internal::HeapValue::isHeapValue(v))
            internal::HeapValue::retain(v);
        return v;
    }

    static inline void release(const Value *v) {
        if (internal::HeapValue::isHeapValue(v))
            internal::HeapValue::release(v);
    }


    template <class T>
    RetainedConst<Value> NewValue(T t) {
        return internal::HeapValue::create(t)->asValue();
    }

} }
