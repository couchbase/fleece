//
//  DB.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
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
#include "MappedFile.hh"
#include "MutableHashTree.hh"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fleece {
    class Encoder;
    class MutableDict;
    class MappedFile;

    /** A persistent key-value store using Fleece. */
    class DB {
    public:
        /** A checkpoint is a kind of timestamp of a database's contents as of some commit.
            It's basically the same as the EOF of the file just after the commit.
            (A new, empty database has a checkpoint of zero.)  */
        using checkpoint_t = uint64_t;

        enum OpenMode {
            kReadOnly  = 0,     // Read-only; file must exist
            kWrite,             // Writeable; file must exist
            kCreateAndWrite,    // Writeable; will create file if it doesn't exist
            kEraseAndWrite,     // Writeable; will erase if file doesn't exist, else create it
        };

        /** The default amount of address space (NOT memory!) reserved by a DB's memory map.
            Multiple DBs on the same file share address space. */
        static constexpr size_t kDefaultMaxSize = 100*1024*1024;

        // Page size; file size will always be rounded to a multiple of this.
        static constexpr size_t kDefaultPageSize = 4*1024;

        // Page size value to use if you don't want pages
        static constexpr size_t kNoPagesSize = 1;

        /** Initializes and opens a DB. Its file will be created if it doesn't exist.
            @param filePath  The filesystem path to the database file.
            @param mode  Determines whether the DB can create and/or write to the file.
            @param maxSize  The amount of address space reserved for the memory-mapped file.
                            The file must not grow larger than this size. */
        DB(const char * NONNULL filePath,
           OpenMode mode =kCreateAndWrite,
           size_t maxSize =kDefaultMaxSize,
           size_t pageSize = kDefaultPageSize);

        /** Initializes and opens a DB, from any checkpoint.
            The value of `checkpoint` can be the other DB's `previousCheckpoint()`, or any
            checkpoint previous to that.
            Since this is historical data, this DB is always opened read-only.
            Changes to the original DB will not affect this one, even if committed.
            (This is an extremely cheap operation, since this DB shares the memory-map with the
            original one. There is no I/O or heap allocation, and only two pages of (mapped)
            memory are read.) */
        DB(const DB&, off_t checkpoint);

        /** Initializes and opens a DB, from the original instance's current checkpoint.
            This instance will be writeable if the original is and if the OpenMode is not
            `kReadOnly`. */
        DB(const DB&, OpenMode =kWrite);

        /** Returns true if the database is writeable, false if it's read-only. */
        bool isWriteable() const                                {return _writeable;}

        /** Returns true if the database is damaged and had to be recovered from an earlier
            checkpoint. The most recent commit(s) might be lost. */
        bool isDamaged() const                                  {return _damaged;}

        /** Returns the database's current checkpoint.
            At any point in the future, if the file has not been compacted, you can open a DB
            at this checkpoint and it will have the exact same contents as that commit. */
        checkpoint_t checkpoint() const                         {return _data.size;}

        /** Returns the database's previous checkpoint, before the last commit. Opening a new DB
            at this checkpoint will make the previous contents accessible.
            If the DB has only been committed once, the previous checkpoint is zero (empty). */
        checkpoint_t previousCheckpoint() const                 {return _prevCheckpoint;}

#pragma mark - DOCUMENT ACCESS

        /** Returns the value of a key, or nullptr. */
        const Dict* get(slice key) const;
        const Dict* get(const char* NONNULL key) const                    {return get(slice(key));}

        /** Returns the value of a key as a MutableDict, so you can modify it. Any changes will
            be saved on the next commit. */
        MutableDict* getMutable(slice key);
        MutableDict* getMutable(const char* NONNULL key)            {return getMutable(slice(key));}

        /** Options for how `put` inserts or replaces values. */
        enum PutMode {
            Insert,     ///< Stores only if no value already exists.
            Upsert,     ///< Stores whether or not a value exists.
            Update,     ///< Stores only if a value already exists.
        };

        using PutCallback = std::function<const Dict*(const Dict*)>;

        /** Stores a new value under a key.
            @param key  The key.
            @param mode  Determines whether this is an insert, upsert, or update operation.
            @param dict  The value, or nullptr to delete the key/value pair.
            @return  True if the value was stored, false if not (according to the `mode`.) */
        bool put(slice key, PutMode mode, const Dict *dict);
        bool put(const char* NONNULL key, PutMode m, const Dict* d) {return put(slice(key),m,d);}
        bool put(slice key, PutMode, PutCallback);
        bool put(const char* NONNULL key, PutMode m, PutCallback c) {return put(slice(key),m,c);}

        /** Removes a key/value.
            @param key  The key to delete.
            @return  True if the key was removed, false if it didn't exist already. */
        bool remove(slice key);
        bool remove(const char* NONNULL key)                        {return remove(slice(key));}

#pragma mark - ITERATOR

        /** Iterator over the keys and values. The order of iteration is arbitrary, since the
         keys are stored in a hash tree. */
        class iterator : MutableHashTree::iterator {
        public:
            using super = MutableHashTree::iterator;
            iterator(DB* NONNULL db)                        :super(db->_tree) { }
            slice key() const                               {return super::key();}
            const Dict* value() const                       {return super::value()->asDict();}
            explicit operator bool() const                  {return super::operator bool();}
            iterator& operator ++()                         {super::operator++(); return *this;}
        };

#pragma mark - COMMIT/REVERT

        /** Saves changes to the file. */
        void commitChanges();

        /** Backs out all changes that have been made since the DB was last committed or opened. */
        void revertChanges();

        /** Writes a copy of the DB to a new file. */
        void writeTo(std::string path);

        using CommitObserver = std::function<void(DB*, checkpoint_t)>;
        void setCommitObserver(CommitObserver co)           {_commitObserver = co;}

#pragma mark - DATA ACCESS:

        slice dataUpToCheckpoint(checkpoint_t) const;
        slice dataSinceCheckpoint(checkpoint_t) const;

        /** Appends data to the file; used to import external changes. */
        bool appendData(off_t offset, slice, bool complete);

    private:
        void loadCheckpoint(checkpoint_t);
        void loadLatest();
        bool validateHeader();
        bool validateTrailer(size_t);
        off_t writeToFile(FILE*, bool deltapages, bool flush);
        void flushFile(FILE*, bool fullSync =false);
        void postCommit(off_t newFileSize);
        bool isLegalCheckpoint(checkpoint_t checkpoint) const;

        Retained<MappedFile> _file;
        size_t _pageSize {kDefaultPageSize};
        slice _data;
        checkpoint_t _prevCheckpoint {0};
        MutableHashTree _tree;
        CommitObserver _commitObserver;
        bool _writeable {true};
        bool _damaged {false};
    };

}
