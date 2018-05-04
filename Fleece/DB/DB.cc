//
//  DB.cc
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
            _tree = MutableHashTree();
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
