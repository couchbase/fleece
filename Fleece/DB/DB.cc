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
        static const uint64_t kMagic = 0x332FFAB5BC644D0C;

        uint32_le padding;
        uint32_le treeOffset;
        uint64_le prevTrailerPos;
        uint64_le magic;
    };


    DB::DB(const char *filePath, size_t maxSize)
    :_file(filePath, "rw+", maxSize)
    {
        load();
    }


    void DB::load() {
        _data = _file.contents();
        if (_data.size == 0) {
            _tree = MutableHashTree();
        } else {
            _damaged = false;
            size_t size = _data.size;
            if (size % kPageSize != 0) {
                _damaged = true;
                size -= size % kPageSize;
            }
            while (!validateDB(size)) {
                _damaged = true;
                size -= kPageSize;
                if (size == 0)
                    FleeceException::_throw(InvalidData, "DB file is corrupted (or not a DB at all)");
            }
        }
    }


    bool DB::validateDB(size_t size) {
        if (size < kPageSize || size % kPageSize != 0)
            return false;
        auto trailer = (const FileTrailer*)&_data[size - sizeof(FileTrailer)];
        if (trailer->magic != FileTrailer::kMagic)
            return false;
        if (trailer->prevTrailerPos >= size || trailer->prevTrailerPos % kPageSize != 0)
            return false;

        ssize_t treePos = size - sizeof(FileTrailer) - trailer->treeOffset;
        if (treePos < 0 || (size_t)treePos < trailer->prevTrailerPos)
            return false;

        _data.setSize(size);
        _tree = HashTree::fromData(slice(_data.buf, treePos));
        return true;
    }


    void DB::revertChanges() {
        load();
    }


    void DB::commitChanges() {
        if (!_tree.isChanged())
            return;
        off_t newFileSize = writeToFile(_file.fileHandle(), true, true);
        _file.resizeTo(newFileSize);
        load();
    }


    void DB::writeTo(string path) {
        FILE *f = fopen(path.c_str(), "w");
        if (!f)
            return;
        writeToFile(f, false, false);
        fclose(f);
    }


    off_t DB::writeToFile(FILE *f, bool delta, bool flush) {
        // Write the delta (or complete file):
        off_t filePos;
        Encoder enc(f);
        enc.suppressTrailer();
        if (delta) {
            filePos = _data.size;
            if (fseeko(f, _data.size, SEEK_SET) < 0)
                FleeceException::_throwErrno("Can't append to file");
            enc.setBase(_data);
        } else {
            filePos = ftello(f);
        }
        _tree.writeTo(enc);
        enc.end();
        filePos += enc.bytesWritten();

        // For some reason, on macOS 10.13.5, calling ftello() at this point will cause problems;
        // that's why I get the position before writing the data. What goes wrong is that the
        // mapped memory is all zeroed out starting from this point (the padding), so of course
        // the trailer is invalid. --jpa

        // Write padding, to position the trailer at the end of a file page:
        int paddingSize = kPageSize - (filePos % kPageSize) - sizeof(FileTrailer);
        if (paddingSize < 0)
            paddingSize += kPageSize;
        for (int i = paddingSize; i > 0; --i)
            fputc(0x55, f);

        // Write the trailer:
        FileTrailer trailer;
        trailer.padding = 0;
        trailer.treeOffset = paddingSize;
        trailer.prevTrailerPos = _data.size;
        trailer.magic = FileTrailer::kMagic;
        fwrite(&trailer, sizeof(trailer), 1, f);

        // Flush bits to disk:
        if (flush) {
            fflush(f);
            fsync(fileno(f));
            //TODO: "full" fsync on Apple platforms, via ioctl
        }

        return filePos + paddingSize + sizeof(trailer);
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

