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
#include "Fleece.hh"
#include <unistd.h>

using namespace std;

namespace fleece {

    static const unsigned kPageSize = 4096;


    // Written at the end of the last page of a file.
    struct FileTrailer {
        uint64_le prevTrailerPos;
        uint32_le magic;
    };


    DB::DB(const char *filePath, size_t maxSize)
    :_file(filePath, "rw+", maxSize)
    {
        load();
    }


    void DB::load() {
        _data = _file.contents();
        if (_data.size > 0)
            _tree = HashTree::fromData(_data);
        else
            _tree = MutableHashTree();
    }


    void DB::revertChanges() {
        load();
    }


    void DB::commitChanges() {
        if (!_tree.isChanged())
            return;
        writeToFile(_file.fileHandle(), true, true);
        _file.resizeToEOF();
        load();
    }


    void DB::writeTo(string path) {
        FILE *f = fopen(path.c_str(), "w");
        if (!f)
            return;
        writeToFile(f, false, false);
        fclose(f);
    }


    void DB::writeToFile(FILE *f, bool delta, bool flush) {
        // Write the delta (or complete file):
        Encoder enc(f);
        if (delta) {
            if (fseeko(f, _data.size, SEEK_SET) < 0)
                FleeceException::_throwErrno("Can't append to file");
            enc.setBase(_data);
        }
        _tree.writeTo(enc);
        enc.end();

        // Write the file trailer:
        unsigned pageFree = kPageSize - (ftello(f) % kPageSize);
        if (pageFree < sizeof(FileTrailer))
            pageFree += kPageSize;
        while (pageFree-- > sizeof(FileTrailer))    // pad to fill up a page
            fputc(0, f);

        FileTrailer trailer;
        fwrite(&trailer, sizeof(trailer), 1, f);

        // Flush bits to disk:
        if (flush) {
            fflush(f);
            fsync(fileno(f));
        }
        //TODO: "full" fsync on Apple platforms w/ioctl
    }


#pragma mark - DOCUMENT ACCESSORS


    const Dict* DB::get(slice key) {
        auto value = _tree.get(key);
        return value ? value->asDict() : nullptr;
    }


    MutableDict* DB::getMutable(slice key) {
        return _tree.getMutableDict(key);
    }


    bool DB::remove(slice key) {
        return _tree.remove(key);
    }


    bool DB::put(slice key, PutMode mode, PutCallback callback) {
        return _tree.insert(key, [&](const Value *curVal) -> const Value* {
            if ((mode == Insert && curVal) || (mode == Update && !curVal))
                return nullptr;
            auto dict = curVal ? curVal->asDict() : nullptr;
            return callback(dict);
        });
    }

    
    bool DB::put(slice key, PutMode mode, const Dict *value) {
        if (value) {
            return _tree.insert(key, [&](const Value *curVal) -> const Value* {
                if ((mode == Insert && curVal) || (mode == Update && !curVal))
                    return nullptr;
                return value;
            });
        } else if (mode != Insert) {
            return _tree.remove(key);
        } else {
            return false;
        }
    }

}

