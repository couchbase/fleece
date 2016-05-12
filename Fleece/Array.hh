//
//  Array.hh
//  Fleece
//
//  Created by Jens Alfke on 5/12/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#ifndef Array_hh
#define Array_hh

#include "Value.hh"

namespace fleece {

    /** A value that's an array. */
    class array : public value {
    public:
        /** The number of items in the array. */
        uint32_t count() const                      {return arrayCount();}

        /** Accesses an array item. Returns NULL for out of range index.
            If you're accessing a lot of items of the same array, it's faster to make an
            iterator and use its sequential or random-access accessors. */
        const value* get(uint32_t index) const;

        /** A stack-based array iterator */
        class iterator {
        public:
            iterator(const array* a);

            /** Returns the number of _remaining_ items. */
            uint32_t count() const                  {return _a.count;}

            const class value* value() const        {return _value;}
            operator const class value* const ()    {return _value;}
            const class value* operator-> ()        {return _value;}

            /** Random access to items. Index is relative to the current item.
                This is very fast, faster than array::get(). */
            const class value* operator[] (unsigned i) {return _a[i];}

            /** Returns false when the iterator reaches the end. */
            explicit operator bool() const          {return _a.count > 0;}

            /** Steps to the next item. (Throws if there are no more items.) */
            iterator& operator++();

            /** Steps forward by one or more items. (Throws if stepping past the end.) */
            iterator& operator += (uint32_t);

        private:
            const class value* rawValue()           {return _a.first;}

            arrayInfo _a;
            const class value *_value;
            friend class value;
        };

        iterator begin() const                      {return iterator(this);}
    };


    /** A value that's a dictionary/map */
    class dict : public value {
    public:
        /** The number of items in the dictionary. */
        uint32_t count() const                      {return arrayCount();}

        /** Looks up the value for a key, assuming the keys are sorted (as they are by default.) */
        const value* get(slice keyToFind) const;

        /** Looks up the value for a key, without assuming the keys are sorted.
            This is slower than get(), but works even if the Fleece data was generated without
            sorted keys. */
        const value* get_unsorted(slice key) const;

#ifdef __OBJC__
        /** Looks up the value for a key given as an NSString object. */
        const value* get(NSString* key) const {
            nsstring_slice keyBytes(key);
            return get(keyBytes);
        }
#endif

        /** A stack-based dictionary iterator */
        class iterator {
        public:
            iterator(const dict*);

            /** Returns the number of _remaining_ items. */
            uint32_t count() const                  {return _a.count;}

            const value* key() const                {return _key;}
            const value* value() const              {return _value;}

            /** Returns false when the iterator reaches the end. */
            explicit operator bool() const          {return _a.count > 0;}

            /** Steps to the next item. (Throws if there are no more items.) */
            iterator& operator ++();

            /** Steps forward by one or more items. (Throws if stepping past the end.) */
            iterator& operator += (uint32_t);

        private:
            void readKV();
            const class value* rawKey()             {return _a.first;}
            const class value* rawValue()           {return _a.second();}

            arrayInfo _a;
            const class value *_key, *_value;
            friend class value;
        };
        
        iterator begin() const                      {return iterator(this);}

        /** An abstracted key for dictionaries. It will cache the key as an encoded value, and it
            will cache the index at which the key was last found, which speeds up succssive
            lookups.
            Warning: An instance of this should be used only on a single thread, and only with
            dictionaries that are stored in the same encoded data. */
        class key {
        public:
            key(slice rawString)                    :_rawString(rawString) { }
            const value* asValue() const            {return _keyValue;}
        private:
            slice const _rawString;
            const value* _keyValue  {nullptr};
            uint32_t _hint          {0xFFFFFFFF};

            friend class dict;
        };

        /** Looks up the value for a key, in a form that can cache the key's Fleece object.
            Using the Fleece object is significantly faster than a normal get. */
        const value* get(key&) const;

    private:
        template <bool WIDE>
            static int keyCmp(const void* keyToFindP, const void* keyP);
        template <bool WIDE>
            const value* get(slice keyToFind) const;
        template <bool WIDE>
        const value* get(const arrayInfo&, key&) const;

        friend class value;
    };

}

#endif /* Array_hh */
