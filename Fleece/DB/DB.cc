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
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

namespace fleece {

    #define Warn(FMT, ...) fprintf(stderr, "WARNING: " FMT "\n", ##__VA_ARGS__)

    // Page size; file size will always be rounded to a multiple of this.
    static const unsigned kPageSize = 4096;


    // Written at the beginning of a file.
    struct FileHeader {
        static constexpr const char* kMagicText = "FleeceDB\n\0\0\0\0\0\0\0";
        static const uint64_t kMagic2 = 0xBAD724227CA1955F;

        char      magicText[14] {"FleeceDB\n\0\0\0\0"};
        uint16_le size          {sizeof(FileHeader)};
        uint64_le magic2        {kMagic2};
    };


    // Written at the end of the last page of a file.
    struct FileTrailer {
        static const uint64_t kMagic1 = 0x332FFAB5BC644D0C;
        static const uint64_t kMagic2 = 0x84A732B5C0E6948B;

        uint64_le magic1            {kMagic1};
        uint32_le treeOffset;       // offset from start of trailer back to end of the HashTree
        uint32_le padding           {0};
        uint64_le prevTrailerPos;   // Absolute file position of previous EOF (end of trailer)
        uint64_le magic2            {kMagic2};

        FileTrailer(uint32_t treeOffset_, uint64_t prevTrailerPos_)
        :treeOffset(treeOffset_)
        ,prevTrailerPos(prevTrailerPos_)
        { }
    };


    DB::DB(const char *filePath, size_t maxSize)
    :_file(filePath, "rw+", maxSize)
    {
        load();
    }


    void DB::load() {
        _data = _file.contents();
        if (_data.size == 0) {
            _damaged = false;
            _tree = MutableHashTree();
        } else {
            _damaged = true;
            size_t size = _data.size;
            if (size < kPageSize) {
                Warn("Not a DB file (too small): %s", _file.path());
                FleeceException::_throw(InvalidData, "Not a DB file (too small)");
            }
            if (!validateHeader()) {
                Warn("Not a DB file; or else header is corrupted: %s", _file.path());
                FleeceException::_throw(InvalidData, "Not a DB file; or else header is corrupted");
            }
            bool damagedSize = false, damagedTrailer = false;
            if (size % kPageSize != 0) {
                Warn("File size 0x%zx is invalid; skipping back to last full page...", size);
                size -= size % kPageSize;
                damagedSize = true;
            }
            while (!validateDB(size)) {
                if (!damagedTrailer) {
                    Warn("Trailer at 0x%zx is invalid; scanning backwards for a valid one...", size);
                    damagedTrailer = true;
                }
                if (size <= kPageSize) {
                    Warn("...no valid trailer found; DB is fatally damaged: %s", _file.path());
                    FleeceException::_throw(InvalidData, "DB file is fatally damaged: no valid trailer found");
                }
                size -= kPageSize;
            }
            if (damagedTrailer || damagedSize)
                Warn("...valid trailer found at 0x%zx; using it", size);
            else
                _damaged = false;
        }
    }


    bool DB::validateHeader() {
        const FileHeader *header = (const FileHeader*)_data.buf;
        return memcmp(header->magicText, FileHeader::kMagicText, sizeof(header->magicText)) == 0
            && header->magic2 == FileHeader::kMagic2
            && header->size < 4096;
    }


    bool DB::validateDB(size_t size) {
        if (size < kPageSize || size % kPageSize != 0)
            return false;
        auto trailer = (const FileTrailer*)&_data[size - sizeof(FileTrailer)];
        if (trailer->magic1 != FileTrailer::kMagic1 || trailer->magic2 != FileTrailer::kMagic2)
            return false;
        if (trailer->prevTrailerPos > size - 4096 || trailer->prevTrailerPos % kPageSize != 0)
            return false;

        ssize_t treePos = size - sizeof(FileTrailer) - trailer->treeOffset;
        if (treePos < 0 || (size_t)treePos < trailer->prevTrailerPos || (treePos % 2))
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


    static void check(int result, const char *msg) {
        if (result < 0)
            FleeceException::_throwErrno(msg);
    }

    static void check_fwrite(FILE *f, const void *data, size_t size) {
        auto written = fwrite(data, size, 1, f);
        if (written < 1)
            FleeceException::_throwErrno("Can't write to file");
    }


    off_t DB::writeToFile(FILE *f, bool delta, bool flush) {
        off_t filePos;
        if (delta) {
            check(fseeko(f, _data.size, SEEK_SET), "Can't append to file");
            filePos = _data.size;
        } else {
            filePos = ftello(f);
        }

        // Write file header:
        if (!delta || _data.size == 0) {
            FileHeader header;
            check_fwrite(f, &header, sizeof(header));
            filePos += sizeof(header);
        }

        // Write the delta (or complete file):
        Encoder enc(f);
        enc.suppressTrailer();
        if (delta)
            enc.setBase(_data);
        _tree.writeTo(enc);
        enc.end();
        filePos += enc.bytesWritten();

        // Extend the file to a page boundary (leaving room for a trailer) and flush everything
        // to disk. This ensures the tree data is 100% durable, before we attempt to write the
        // trailer that marks it as valid.
        off_t finalPos = filePos + sizeof(FileTrailer);
        finalPos += kPageSize - (finalPos % kPageSize);
        check(ftruncate(fileno(f), finalPos), "Can't grow the file");

        if (flush)
            flushFile(f, true);

        // Write the trailer:
        FileTrailer trailer(uint32_t(finalPos - sizeof(FileTrailer) - filePos),
                            _data.size);
        fseeko(f, -sizeof(FileTrailer), SEEK_END);
        check_fwrite(f, &trailer, sizeof(trailer));

        // Flush again to make sure the header is durably saved:
        if (flush)
            flushFile(f);

        return finalPos;
    }


    void DB::flushFile(FILE *f, bool fullSync) {
        // Adapted from SQLite source code
        check(fflush(f), "Can't flush file");
#ifdef F_FULLFSYNC
        if( fullSync && fcntl(fileno(f), F_FULLFSYNC, 0) == 0)
            return;
#endif
        // If the FULLFSYNC failed, or isn't supported, fall back to fsync().
        if (fsync(fileno(f)) < 0)
            Warn("DB failed to flush file to disk (errno=%d)", errno);
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

