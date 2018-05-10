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
        static constexpr size_t kDefaultMaxSize = 100*1024*1024;

        /** Initializes and opens a DB. Its file will be created if it doesn't exist.
            @param filePath  The path to the database file.
            @param maxSize  The amount of address space reserved for the memory-mapped file.
                            The file must not grow larger than this size. */
        DB(const char * NONNULL filePath, size_t maxSize =kDefaultMaxSize);

        /** Returns the value of a key, or nullptr. */
        const Dict* get(slice key);
        const Dict* get(const char* NONNULL key)                    {return get(slice(key));}

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

        /** Saves changes to the file. */
        void commitChanges();

        /** Backs out all changes that have been made since the DB was last committed or opened. */
        void revertChanges();

        /** Writes a copy of the DB to a new file. */
        void writeTo(std::string path);

        /** Returns the total size of the DB on disk. */
        size_t dataSize() const                                 {return _file.contents().size;}

        bool isDamaged()                                        {return _damaged;}


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

    private:
        void load();
        bool validateDB(size_t);
        off_t writeToFile(FILE*, bool deltapages, bool flush);
        
        MappedFile _file;
        slice _data;
        MutableHashTree _tree;
        bool _damaged {false};
    };

}
