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
#include "slice.hh"
#include "MutableHashTree.hh"
#include "sliceIO.hh"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fleece {
    class Encoder;
    class mmap_slice;

    class DB {
    public:
        DB(std::string filePath);

        const Dict* get(slice key);
        const Dict* get(const char* NONNULL key)                    {return get(slice(key));}

        MutableDict* getMutable(slice key);
        MutableDict* getMutable(const char* NONNULL key)            {return getMutable(slice(key));}

        enum PutMode {
            Insert,
            Upsert,
            Update,
        };

        using PutCallback = std::function<const Dict*(const Dict*)>;

        bool put(slice key, PutMode, const Dict*);
        bool put(const char* NONNULL key, PutMode m, const Dict* d) {return put(slice(key),m,d);}
        bool put(slice key, PutMode, PutCallback);
        bool put(const char* NONNULL key, PutMode m, PutCallback c) {return put(slice(key),m,c);}

        bool remove(slice key);
        bool remove(const char* NONNULL key)                        {return remove(slice(key));}

        void saveChanges();
        void writeTo(std::string path);

        size_t dataSize() const                                     {return _data.size;}
        
    private:
        void load();
        
        std::string _filePath;
        mmap_slice _data;
        MutableHashTree _tree;
    };

}
