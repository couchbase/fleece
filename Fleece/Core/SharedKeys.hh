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
#include "StringTable.hh"
#include <vector>


namespace fleece {

    /** Keeps track of a set of dictionary keys that are stored in abbreviated (small integer) form.

        Encoders can be configured to use an instance of this, and will use it to abbreviate keys
        that are given to them as strings. (Note: This class is not thread-safe!)

        The Dict class does _not_ use this; it has no outside context to be able to find shared
        state such as this object. The client is responsible for using this object to map between
        string and integer keys when using Dicts that were encoded this way. */
    class SharedKeys {
    public:

        SharedKeys() { }
        virtual ~SharedKeys();

        /** Sets the maximum number of keys that can be stored in the mapping. After this number is
            reached, `encode` won't add any new strings. (Defaults to 2048.) */
        void setMaxCount(size_t m)              {_maxCount = m;}

        /** Sets the maximum length of string that can be mapped. (Defaults to 16 bytes.) */
        void setMaxKeyLength(size_t m)          {_maxKeyLength = m;}

        /** The number of stored keys. */
        size_t count() const                    {return _table.count();}

        /** Maps a string to an integer, or returns false if there is no mapping. */
        bool encode(slice string, int &key) const;

        /** Maps a string to an integer. Will automatically add a new mapping if the string
         qualifies. */
        bool encodeAndAdd(slice string, int &key);

        /** Decodes an integer back to a string. */
        slice decode(int key) const;

        /** A vector whose indices are encoded keys and values are the strings. */
        const std::vector<alloc_slice>& byKey() const   {return _byKey;}

        /** Reverts the mapping to an earlier state by removing the mappings with keys greater than
            or equal to the new count. (I.e. it truncates the byKey vector.) */
        void revertToCount(size_t count);

        /** Determines whether a new string should be added. Default implementation returns true
            if the string contains only alphanumeric characters, '_' or '-'. */
        virtual bool isEligibleToEncode(slice str);

        bool isUnknownKey(int key) const                {return key >= (int)_byKey.size();}

        virtual bool refresh()                          {return false;}

        static const size_t kDefaultMaxCount = 2048;        // Max number of keys to store
        static const size_t kDefaultMaxKeyLength = 16;      // Max length of string to store

        typedef const void* PlatformString;

        /** Allows an uninterpreted value (like a pointer to a platform String object) to be
            associated with an encoded key. */
        void setPlatformStringForKey(int key, PlatformString) const;
        PlatformString platformStringForKey(int key) const;

    private:
        friend class PersistentSharedKeys;

        virtual int add(slice string);

        fleece::StringTable _table;                     // Hash table mapping slice->int
        std::vector<alloc_slice> _byKey;                // Reverse mapping, int->slice
        std::vector<PlatformString> _platformStringsByKey; // Reverse mapping, int->platform key
        size_t _maxCount {kDefaultMaxCount};            // Max number of strings I will hold
        size_t _maxKeyLength {kDefaultMaxKeyLength};    // Max length of string I will add
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
            Call if aborting a transaction, or a transaction failed to commit. */
        void revert();

        /** Call this after a transaction ends, after calling save() or revert(). */
        void transactionEnded();

        /** Returns true if the table has changed from its persisted state. */
        bool changed() const                    {return _persistedCount < count();}

    protected:
        /** Abstract: Should read the persisted data and call loadFrom() with it. */
        virtual bool read() =0;

        /** Abstract: Should write the given encoded data to persistent storage. */
        virtual void write(slice encodedData) =0;

        /** Updates state given previously-persisted data. */
        bool loadFrom(slice fleeceData);

    private:
        virtual int add(slice str) override;

        size_t _persistedCount {0};             // Number of strings written to storage
        size_t _committedPersistedCount {0};    // Number of strings written to storage & committed
        bool _inTransaction {false};            // True during a transaction
    };
}
