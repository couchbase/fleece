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
#include "FileUtils.hh"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

namespace fleece {

    #define Warn(FMT, ...) fprintf(stderr, "WARNING: " FMT "\n", ##__VA_ARGS__)


    // Written at the beginning of a file.
    struct FileHeader {
        static constexpr const char* kMagicText = "FleeceDB\n\0\0\0\0\0\0\0";
        static const uint64_t kMagic2 = 0xBBD724227CA1955F;

        char      magicText[14];
        uint16_le size          {sizeof(FileHeader)};
        uint64_le magic2        {kMagic2};
        uint32_le pageSize      {DB::kDefaultPageSize};

        FileHeader(uint32_t pageSize_)
        :pageSize(pageSize_)
        {
            memcpy(magicText, "FleeceDB\n\0\0\0\0", sizeof(magicText));
        }
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


    static const char* kModeStrings[4] = {"r", "r+", "rw+", "w+"};


    DB::DB(const char *filePath, OpenMode mode, size_t maxSize, size_t pageSize)
    :_file( new MappedFile(filePath,
                           kModeStrings[mode],
                           maxSize) )
    ,_pageSize(pageSize)
    ,_treeData(_file->contents())
    ,_writeable(mode > kReadOnly)
    {
        assert(pageSize > 0);
        loadLatest();
    }


    DB::DB(const DB &other, OpenMode mode)
    :_file(other._file)
    ,_treeData(other._treeData)
    ,_writeable(other._writeable && mode > kReadOnly)
    {
        loadCheckpoint(other.checkpoint());
    }



    DB::DB(const DB &other, off_t checkpoint)
    :_file(other._file)
    ,_treeData(other._treeData)
    ,_writeable(false)
    {
        assert(checkpoint <= (off_t)_treeData.size);
        loadCheckpoint(checkpoint);
    }


    void DB::loadLatest() {
        loadCheckpoint(_file->contents().size);
    }

    void DB::loadCheckpoint(checkpoint_t checkpoint) {
        if (checkpoint > SIZE_MAX)
            throw std::logic_error("Checkpoint too large for address space");
        _treeData.setSize((size_t)checkpoint);
        if (checkpoint == 0) {
            _damaged = false;
            _tree = MutableHashTree();
        } else {
            // Validate the file header:
            _damaged = true;
            size_t size = _treeData.size;
            if (!validateHeader()) {
                Warn("Not a DB file; or else header is corrupted: %s", _file->path());
                FleeceException::_throw(InvalidData, "Not a DB file; or else header is corrupted");
            }

            // Look for the last valid trailer; usually at EOF unless the last save failed:
            bool damagedSize = false, damagedTrailer = false;
            if (size % _pageSize != 0) {
                Warn("File size 0x%zx is invalid; skipping back to last full page...", size);
                size -= size % _pageSize;
                damagedSize = true;
            }
            ssize_t treePos;
            while ((treePos = validateTrailer(size)) < 0) {
                if (!damagedTrailer && _pageSize > 1) {
                    Warn("Trailer at 0x%zx is invalid; scanning backwards for a valid one...", size);
                    damagedTrailer = true;
                }
                if (size <= _pageSize || _pageSize == 1) {
                    Warn("...no valid trailer found; DB is fatally damaged: %s", _file->path());
                    FleeceException::_throw(InvalidData, "DB file is fatally damaged: no valid trailer found");
                }
                size -= _pageSize;
            }

            // OK, file can be used, so load tree:
            if (damagedTrailer || damagedSize)
                Warn("...valid trailer found at 0x%zx; using it", size);
            else
                _damaged = false;
            _treeData.setSize(size);
            _tree = HashTree::fromData(slice(_treeData.buf, treePos));
        }
    }


    bool DB::validateHeader() {
        const FileHeader *header = (const FileHeader*)_treeData.buf;
        if ( _treeData.size >= sizeof(FileHeader)
                 && memcmp(header->magicText, FileHeader::kMagicText, sizeof(header->magicText)) == 0
                 && header->magic2 == FileHeader::kMagic2
                 && header->size >= sizeof(FileHeader)
                 && header->size < max((uint32_t)header->pageSize, (uint32_t)4096)
                 && header->pageSize <= kMaxPageSize
                 && header->pageSize <= _treeData.size) {
            _pageSize = header->pageSize;
            return true;
        } else {
            return false;
        }
    }


    ssize_t DB::validateTrailer(size_t size) {
        if (size < _pageSize || size % _pageSize != 0)
            return -1;
        auto trailer = (const FileTrailer*)&_treeData[size - sizeof(FileTrailer)];
        if (trailer->magic1 != FileTrailer::kMagic1 || trailer->magic2 != FileTrailer::kMagic2)
            return -1;

        checkpoint_t prevTrailerPos = trailer->prevTrailerPos;
        if (prevTrailerPos > size - _pageSize || prevTrailerPos % _pageSize != 0)
            return -1;

        ssize_t treePos = size - sizeof(FileTrailer) - trailer->treeOffset;
        if (treePos < 0 || (size_t)treePos < trailer->prevTrailerPos || (treePos % 2))
            return -1;

        _prevCheckpoint = prevTrailerPos;
        return treePos;
    }


    void DB::revertChanges() {
        loadCheckpoint(checkpoint());
    }


    void DB::commitChanges() {
        if (!_tree.isChanged())
            return;
        assert(_writeable);
        off_t newFileSize = writeToFile(_file->fileHandle(), true, true);
        postCommit(newFileSize);
    }


    void DB::postCommit(off_t newFileSize) {
        _file->resizeTo(newFileSize);
        loadCheckpoint(newFileSize);

        if (_commitObserver)
            _commitObserver(this, newFileSize);
    }


    void DB::writeTo(string path) {
        FILE *f = fopen(path.c_str(), "w");
        if (!f)
            return;
        writeToFile(f, false, false);
        fclose(f);
    }


    off_t DB::writeToFile(FILE *f, bool delta, bool flush) {
        off_t filePos;
        if (delta) {
            checkErrno(fseeko(f, _treeData.size, SEEK_SET), "Can't append to file");
            filePos = _treeData.size;
        } else {
            filePos = ftello(f);
        }

        // Write file header:
        if (!delta || _treeData.size == 0) {
            FileHeader header((uint32_t)_pageSize);
            check_fwrite(f, &header, sizeof(header));
            filePos += sizeof(header);
        }

        // Write the delta (or complete file):
        Encoder enc(f);
        enc.suppressTrailer();
        if (delta)
            enc.setBase(_treeData);
        _tree.writeTo(enc);
        enc.end();
        filePos += enc.bytesWritten();

        // Extend the file to a page boundary (leaving room for a trailer) and flush everything
        // to disk. This ensures the tree data is 100% durable, before we attempt to write the
        // trailer that marks it as valid.
        off_t finalPos = filePos + sizeof(FileTrailer);
        if (finalPos % _pageSize != 0)
            finalPos += _pageSize - (finalPos % _pageSize);
#ifdef FL_ESP32
        for (auto i = filePos; i < finalPos - sizeof(FileTrailer); ++i)
            fputc(0, f);
#else
        checkErrno(ftruncate(fileno(f), finalPos), "Can't grow the file");
#endif

        if (flush)
            flushFile(f, true);

        // Write the trailer:
        FileTrailer trailer(uint32_t(finalPos - sizeof(FileTrailer) - filePos),
                            (delta ? _treeData.size : 0));
#ifndef FL_ESP32
        checkErrno(fseeko(f, -sizeof(FileTrailer), SEEK_END), "seek");
#endif
        check_fwrite(f, &trailer, sizeof(trailer));

        // Flush again to make sure the header is durably saved:
        if (flush)
            flushFile(f);

        return finalPos;
    }


    void DB::flushFile(FILE *f, bool fullSync) {
        // Adapted from SQLite source code
        checkErrno(fflush(f), "Can't flush file");
#ifndef FL_ESP32
#ifdef F_FULLFSYNC
        if( fullSync && fcntl(fileno(f), F_FULLFSYNC, 0) == 0)
            return;
#endif
        // If the FULLFSYNC failed, or isn't supported, fall back to fsync().
        if (fsync(fileno(f)) < 0)
            Warn("DB failed to flush file to disk (errno=%d)", errno);
#endif
    }


#pragma mark - DOCUMENT ACCESSORS


    const Dict* DB::get(slice key) const {
        auto value = _tree.get(key);
        return value ? value->asDict() : nullptr;
    }


    MutableDict* DB::getMutable(slice key) {
        assert(_writeable);
        return _tree.getMutableDict(key);
    }


    bool DB::remove(slice key) {
        assert(_writeable);
        return _tree.remove(key);
    }


    bool DB::put(slice key, PutMode mode, PutCallback callback) {
        assert(_writeable);
        return _tree.insert(key, [&](const Value *curVal) -> const Value* {
            if ((mode == Insert && curVal) || (mode == Update && !curVal))
                return nullptr;
            auto dict = curVal ? curVal->asDict() : nullptr;
            return callback(dict);
        });
    }

    
    bool DB::put(slice key, PutMode mode, const Dict *value) {
        assert(_writeable);
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


#pragma mark - DATA ACCESS


    bool DB::isLegalCheckpoint(checkpoint_t checkpoint) const {
        return checkpoint <= _treeData.size && checkpoint % _pageSize == 0;
    }


    slice DB::dataUpToCheckpoint(checkpoint_t checkpoint) const {
        if (!isLegalCheckpoint(checkpoint))
            return nullslice;
        return _treeData.upTo(checkpoint);
    }


    slice DB::dataSinceCheckpoint(checkpoint_t checkpoint) const {
        if (!isLegalCheckpoint(checkpoint))
            return nullslice;
        return _treeData.from(checkpoint);
    }


    bool DB::appendData(off_t offset, slice data, bool complete) {
        FILE *f = _file->fileHandle();
        if (check_getEOF(f) != offset)
            return false;
        check_fwrite(f, data.buf, data.size);
        if (complete) {
            fflush(f);
            postCommit(offset + data.size);
        }
        return true;
    }

}

