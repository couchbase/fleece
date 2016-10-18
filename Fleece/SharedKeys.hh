//
//  SharedKeys.hh
//  Fleece
//
//  Created by Jens Alfke on 10/17/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include "StringTable.hh"
#include <vector>


namespace fleece {

    /** Keeps track of a persistent set of dictionary keys that are abbreviated to small integers.

        An Encoder can be configured to use an instance of this, and will use it to abbreviate keys
        that are given to it as strings.

        The Dict class does _not_ use this; it has no outside context to be able to find shared
        state such as this object. The client is responsible for using this object to map between
        string and integer keys when using Dicts that were encoded this way.
     
        Note: This is an abstract class. You must implement the read() and write() methods to
        implement the actual persistence. */
    class SharedKeys {
    public:

        SharedKeys() { }
        virtual ~SharedKeys();

        //////// ENCODING/DECODING:

        /** The number of stored keys. */
        size_t count() const                    {return _table.count();}

        /** Maps a string to an integer. Will automatically add a new mapping if the string
         qualifies. Can only be called while in a transaction. */
        bool encode(slice string, int &key);

        /** Decodes an integer back to a string. */
        slice decode(int key);

        //////// PERSISTENCE:

        /** Updates state from persistent storage. Not usually necessary. */
        void update();

        /** Call this right after a transaction has started; it enables adding new strings. */
        void transactionBegan();

        /** Writes any changed state. Call before committing a transaction. */
        void save();

        /** Reverts to persisted state as of the end of the last transaction.
            Call if aborting a transaction. */
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
        int add(slice string);

        fleece::StringTable _table;
        std::vector<alloc_slice> _byKey;
        size_t _persistedCount {0};
        size_t _committedPersistedCount {0};
        bool _inTransaction {false};
    };
}
