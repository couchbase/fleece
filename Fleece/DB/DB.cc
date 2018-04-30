//
//  DB.cc
//  Fleece
//
//  Created by Jens Alfke on 4/27/18.
//Copyright © 2018 Couchbase. All rights reserved.
//

#include "DB.hh"
#include "sliceIO.hh"
#include "Fleece.hh"

using namespace std;

namespace fleece {

    DB::DB(string filePath)
    :_filePath(filePath)
    {
        load();
    }

    void DB::load() {
        _unsavedValues.clear();
        _data = mmap_slice(_filePath.c_str());
        if (_data)
            _tree = HashTree::fromData(_data);
        else
            _tree = MHashTree();
    }

    void DB::saveChanges() {
        if (_tree.isChanged()) {
            Encoder enc;
            enc.setBase(_data);
            _tree.writeTo(enc);
            alloc_slice data = enc.extractOutput();
            appendToFile(data, _filePath.c_str());

            load();
        }
    }

    void DB::writeTo(string path) {
        Encoder enc;
        _tree.writeTo(enc);
        alloc_slice data = enc.extractOutput();
        writeToFile(data, path.c_str());
    }


    const Value* DB::get(slice key) {
        return _tree.get(key);
    }


    bool DB::remove(slice key) {
        return _tree.remove(key);
    }


    bool DB::put(slice key, PutMode mode, PutCallback callback) {
        if (!_enc)
            _enc.reset(new Encoder);
        return _tree.insert(key, [&](const Value *curVal) -> const Value* {
            if ((mode == Insert && curVal) || (mode == Update && !curVal))
                return nullptr;
            if (!callback(curVal, *_enc)) {
                _enc->reset();
                return nullptr;
            }
            auto valueData = _enc->extractOutput();
            _unsavedValues.push_back(valueData);
            return Value::fromTrustedData(valueData);
        });
    }

    
    bool DB::put(slice key, PutMode mode, const Value *value) {
        return _tree.insert(key, [&](const Value *curVal) -> const Value* {
            if ((mode == Insert && curVal) || (mode == Update && !curVal))
                return nullptr;
            return value;
        });
    }

}
