//
// Writer.cc
//
// Copyright (c) 2015 Couchbase, Inc All rights reserved.
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

#include "Writer.hh"
#include "PlatformCompat.hh"
#include "FleeceException.hh"
#include "decode.h"
#include "encode.h"
#include <algorithm>
#include "betterassert.hh"


namespace fleece {

    Writer::Writer(size_t initialCapacity)
    :_chunkSize(initialCapacity)
    ,_outputFile(nullptr)
    {
        addChunk(initialCapacity);
    }


    Writer::Writer(FILE *outputFile)
    :Writer(kDefaultInitialCapacity)
    {
        assert(outputFile);
        _outputFile = outputFile;
    }


    Writer::Writer(Writer&& w) noexcept
    :_available(std::move(w._available))
    ,_chunks(std::move(w._chunks))
    ,_chunkSize(w._chunkSize)
    ,_length(w._length)
    ,_outputFile(w._outputFile)
    {
        migrateInitialBuf(w);
        memcpy(_initialBuf, w._initialBuf, sizeof(_initialBuf));
        w._outputFile = nullptr;
    }


    Writer::~Writer() {
        if (_outputFile)
            flush();
        for (auto &chunk : _chunks)
            freeChunk(chunk);
    }


    Writer& Writer::operator= (Writer&& w) noexcept {
        _available = std::move(w._available);
        _length = w._length;
        _chunks = std::move(w._chunks);
        migrateInitialBuf(w);
        _outputFile = w._outputFile;
        memcpy(_initialBuf, w._initialBuf, sizeof(_initialBuf));
        w._outputFile = nullptr;
        return *this;
    }


    void Writer::_reset() {
        if (_outputFile)
            return;

        size_t nChunks = _chunks.size();
        if (nChunks > 1) {
            for (size_t i = 0; i < nChunks-1; i++)
                freeChunk(_chunks[i]);
            _chunks.erase(_chunks.begin(), _chunks.end() - 1);
        }
        _available = _chunks[0];
    }


    void Writer::reset() {
        _reset();
        _length = _available.size;  // effectively 0
    }


#if DEBUG
    void Writer::assertLengthCorrect() const {
        if (!_outputFile) {
            size_t len = 0;
            forEachChunk([&](slice chunk) {
                len += chunk.size;
            });
            assert(len == length());
        }
    }
#endif


    const void* Writer::writeToNewChunk(slice s) {
        // If we got here, a call to write(s) would not fit in the current chunk
        if (_outputFile) {
            flush();
            if (s.size > _chunkSize) {
                freeChunk(_chunks.back());
                _chunks.clear();
                addChunk(s.size);
            }
            _length -= _available.size;
            _available = _chunks[0];
            _length += _available.size;
        } else {
            if (_usuallyTrue(_chunkSize <= 64*1024))
                _chunkSize *= 2;
            addChunk(std::max(s.size, _chunkSize));
        }

        // Now that we have room, write:
        auto result = _available.buf;
        if (s.buf)
            ::memcpy((void*)_available.buf, s.buf, s.size);
        _available.moveStart(s.size);
        return result;
    }


    void Writer::flush() {
        if (!_outputFile)
            return;
        auto chunk = _chunks.back();
        size_t writtenLength = chunk.size - _available.size;
        if (writtenLength > 0) {
            _length -= _available.size;
            if (fwrite(chunk.buf, 1, writtenLength, _outputFile) < writtenLength)
                FleeceException::_throwErrno("Writer can't write to file");
            _available = chunk;
            _length += _available.size;
        }
        assertLengthCorrect();
    }


    void Writer::addChunk(size_t capacity) {
        _length -= _available.size;
        if (!_chunks.empty()) {
            auto &last = _chunks.back();
            // (should I realloc() it?)
            last.setSize(last.size - _available.size);
        }
        if (_chunks.empty() && capacity <= kDefaultInitialCapacity)
            _available = _chunks.emplace_back(_initialBuf, sizeof(_initialBuf));
        else
            _available = _chunks.emplace_back(slice::newBytes(capacity), capacity);
        _length += _available.size;
    }


    void Writer::freeChunk(slice chunk) {
        if (chunk.buf != &_initialBuf)
            chunk.free();
    }

    void Writer::migrateInitialBuf(const Writer& other) {
        // If a simple std::move is used for _chunks, there will be a leftover
        // garbage entry pointing to the old initial buffer of the previous
        // object. Replace it with the current initial buf.
        int pos = 0;
        for(auto& chunk : _chunks) {
            if(chunk.buf == other._initialBuf) {
                _chunks[pos] = slice(_initialBuf, sizeof(_initialBuf));
                return;
            }

            pos++;
        }
    }



    std::vector<slice> Writer::output() const {
        std::vector<slice> result;
        result.reserve(_chunks.size());
        forEachChunk([&](slice chunk) {
            result.push_back(chunk);
        });
        return result;
    }


    alloc_slice Writer::finish() {
        alloc_slice output;
        if (_outputFile) {
            flush();
        } else {
            output = alloc_slice(length());
            void* dst = (void*)output.buf;
            forEachChunk([&](slice chunk) {
                memcpy(dst, chunk.buf, chunk.size);
                dst = offsetby(dst, chunk.size);
            });
            reset();
        }
        assertLengthCorrect();
        return output;
    }


    bool Writer::writeOutputToFile(FILE *f) {
        assert(!_outputFile);
        bool result = true;
        forEachChunk([&](slice chunk) {
            if (result && fwrite(chunk.buf, chunk.size, 1, f) < chunk.size)
                result = false;
        });
        if (result) {
            auto len = length();
            _reset();       // don't reset _length, so offsets remain consistent after
            _length = len - _available.size;
        }
        return result;
    }


#pragma mark - BASE64:


    void Writer::writeBase64(slice data) {
        size_t base64size = ((data.size + 2) / 3) * 4;
        char *dst;
        if (_outputFile)
            dst = (char*)slice::newBytes(base64size);
        else
            dst = (char*)reserveSpace(base64size);
        base64::encoder enc;
        enc.set_chars_per_line(0);
        size_t written = enc.encode(data.buf, data.size, dst);
        written += enc.encode_end(dst + written);
        if (_outputFile) {
            write(dst, written);
            free(dst);
        }
        assert((size_t)written == base64size);
        (void)written;      // suppresses 'unused value' warning in release builds
    }


    void Writer::writeDecodedBase64(slice base64) {
        base64::decoder dec;
        std::vector<char> buf((base64.size + 3) / 4 * 3);
        size_t len = dec.decode(base64.buf, base64.size, buf.data());
        write(buf.data(), len);
    }

}
