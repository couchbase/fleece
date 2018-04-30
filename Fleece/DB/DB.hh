//
//  DB.hh
//  Fleece
//
//  Created by Jens Alfke on 4/27/18.
//Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "slice.hh"
#include "MHashTree.hh"
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

        const Value* get(slice key);

        enum PutMode {
            Insert,
            Upsert,
            Update,
        };
        using PutCallback = std::function<bool(const Value*, Encoder&)>;

        bool put(slice key, PutMode, const Value*);
        bool put(slice key, PutMode, PutCallback);

        bool remove(slice key);

        void saveChanges();
        void writeTo(std::string path);

        size_t dataSize() const                         {return _data.size;}
        
    private:
        void load();
        
        std::string _filePath;
        mmap_slice _data;
        MHashTree _tree;
        std::unique_ptr<Encoder> _enc;
        std::vector<alloc_slice> _unsavedValues;
    };

}
