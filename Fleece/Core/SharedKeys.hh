//
// SharedKeys.hh
//
// Copyright (c) 2016 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once
#include "RefCounted.hh"
#include "ConcurrentMap.hh"
#include <array>
#include <mutex>
#include <vector>
#include "betterassert.hh"

#ifdef __APPLE__
#include <CoreFoundation/CFString.h>
#endif


namespace fleece { namespace impl {
    class Value;


    /** A Dict key that may be either a string or a small integer. */
    class key_t {
    public:
        key_t()                                     { }
        key_t(slice key)        :_string(key)       {assert_precondition(key);}
        key_t(int key)          :_int((int16_t)key) {assert_precondition(key >= 0 && key <= INT16_MAX);}

        key_t(const Value *v) noexcept;

        bool shared() const FLPURE     {return !_string;}
        int asInt() const FLPURE       {assert_precondition(shared()); return _int;}
        slice asString() const FLPURE  {return _string;}

        bool operator== (const key_t &k) const noexcept FLPURE;
        bool operator< (const key_t &k) const noexcept FLPURE;

    private:
        slice _string;
        int16_t _int {-1};
    };


    /** Keeps track of a set of dictionary keys that are stored in abbreviated (small integer) form.

        Encoders can be configured to use an instance of this, and will use it to abbreviate keys
        that are given to them as strings.

        When Fleece data that uses shared keys is loaded, a Scope or Doc object must be instantiated
        to record the SharedKeys instance associated with it. When a Dict access results in an
        integer key, the Dict will look up a Scope responsible for its address, and get the
        SharedKeys instance from that Scope.

        NOTE: This class is now thread-safe. */
    class SharedKeys : public RefCounted {
    public:
        SharedKeys();
        explicit SharedKeys(slice stateData)            :SharedKeys() {loadFrom(stateData);}

        alloc_slice stateData() const;

        /** Sets the maximum length of string that can be mapped. (Defaults to 16 bytes.) */
        void setMaxKeyLength(size_t m)          {_maxKeyLength = m;}

        /** The number of stored keys. */
        size_t count() const FLPURE;

        /** Maps a string to an integer, or returns false if there is no mapping. */
        bool encode(slice string, int &key) const;

        /** Maps a string to an integer. Will automatically add a new mapping if the string
            qualifies. */
        bool encodeAndAdd(slice string, int &key);

        /** Returns true if the string could be added, i.e. there's room, it's not too long,
            and it has only valid characters. */
        inline bool couldAdd(slice str) const FLPURE {
            return count() < kMaxCount && str.size <= _maxKeyLength && isEligibleToEncode(str);
        }

        /** Decodes an integer back to a string. */
        slice decode(int key) const;

        /** A vector whose indices are encoded keys and values are the strings. */
        std::vector<slice> byKey() const;

        /** Reverts the mapping to an earlier state by removing the mappings with keys greater than
            or equal to the new count. (I.e. it truncates the byKey vector.) */
        void revertToCount(size_t count);

        bool isUnknownKey(int key) const FLPURE;

        virtual bool refresh()                          {return false;}

        static const size_t kMaxCount = 2048;               // Max number of keys to store
        static const size_t kDefaultMaxKeyLength = 16;      // Max length of string to store

#ifdef __APPLE__
        typedef CFStringRef PlatformString;
#else
        typedef const void* PlatformString;
#endif

        /** Allows an uninterpreted value (like a pointer to a platform String object) to be
            associated with an encoded key. */
        void setPlatformStringForKey(int key, PlatformString) const;
        PlatformString platformStringForKey(int key) const;

    protected:
        virtual ~SharedKeys();
        virtual bool loadFrom(slice stateData);

        /** Determines whether a new string should be added. Default implementation returns true
            if the string contains only alphanumeric characters, '_' or '-'. */
        virtual bool isEligibleToEncode(slice str) const FLPURE;

    private:
        friend class PersistentSharedKeys;

        bool _encodeAndAdd(slice string, int &key);
        virtual bool _add(slice string, int &key);
        bool _isUnknownKey(int key) const FLPURE        {return (size_t)key >= _count;}
        slice decodeUnknown(int key) const;

        size_t _maxKeyLength {kDefaultMaxKeyLength};    // Max length of string I will add
        mutable std::mutex _mutex;
        unsigned _count {0};
        mutable std::vector<PlatformString> _platformStringsByKey; // Reverse mapping, int->platform key
        ConcurrentMap _table;                             // Hash table mapping slice->int
        std::array<slice, kMaxCount> _byKey;      // Reverse mapping, int->slice
    };



    /** Subclass of SharedKeys that supports persistence of the string-to-int mapping via some
        kind of transactional storage.

        Note: This is an abstract class. You must implement the read() and write() methods to
        implement the actual persistence. */
    class PersistentSharedKeys : public SharedKeys {
    public:
        PersistentSharedKeys();

        /** Updates state from persistent storage. Not usually necessary. */
        virtual bool refresh() override;

        /** Call this right after a transaction has started; it enables adding new strings. */
        void transactionBegan();

        /** Writes any changed state. Call before committing a transaction. */
        void save();

        /** Reverts to persisted state as of the end of the last transaction.
            Call when aborting a transaction, or a transaction failed to commit.
            @warning  Any use of encoded keys created during the transaction will
                      lead to "undefined behavior". */
        void revert();

        /** Call this after a transaction ends, after calling save() or revert(). */
        void transactionEnded();

        /** Returns true if the table has changed from its persisted state. */
        bool changed() const FLPURE                    {return _persistedCount < count();}

    protected:
        /** Abstract: Should read the persisted data and call loadFrom() with it. */
        virtual bool read() =0;

        /** Abstract: Should write the given encoded data to persistent storage. */
        virtual void write(slice encodedData) =0;

        /** Updates state given previously-persisted data. */
        bool loadFrom(slice fleeceData) override;

        std::mutex _refreshMutex;

    private:
        virtual bool _add(slice str, int &key) override;

        size_t _persistedCount {0};             // Number of strings written to storage
        size_t _committedPersistedCount {0};    // Number of strings written to storage & committed
        bool _inTransaction {false};            // True during a transaction
    };
} }
