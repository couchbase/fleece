//
//  Writer.cc
//  Fleece
//
//  Created by Jens Alfke on 2/4/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "Writer.hh"
#include "PlatformCompat.hh"
#include "decode.h"
#include "encode.h"
#include <assert.h>
#include <algorithm>


namespace fleece {

    Writer::Writer(size_t initialCapacity)
    :_chunkSize(initialCapacity),
     _length(0)
    {
        if (initialCapacity <= kDefaultInitialCapacity)
            _chunks.emplace_back(_initialBuf, sizeof(_initialBuf));
        else
            addChunk(initialCapacity);
    }

    Writer::Writer(Writer&& w) noexcept
    :_chunks(std::move(w._chunks))
    {
        w._chunks.clear();
    }

    Writer::~Writer() {
        for (auto &chunk : _chunks)
            freeChunk(chunk);
    }

    Writer& Writer::operator= (Writer&& w) noexcept {
        _chunks = std::move(w._chunks);
        w._chunks.clear();
        return *this;
    }

    void Writer::reset() {
        size_t size = _chunks.size();
        if (size == 0) {
            addChunk(_chunkSize);
        } else {
            if (size > 1) {
                for (size_t i = 0; i < size-1; i++)
                    freeChunk(_chunks[i]);
                _chunks.erase(_chunks.begin(), _chunks.end() - 1);
            }
            _chunks[0].reset();
        }
        _length = 0;
    }

    const void* Writer::curPos() const {
        return _chunks.back().available().buf;
    }

    size_t Writer::posToOffset(const void *pos) const {
        size_t offset = 0;
        for (auto &chunk : _chunks) {
            if (chunk.contains(pos))
                return offset + chunk.offsetOf(pos);
            offset += chunk.length();
        }
        throw std::out_of_range("invalid pos for Writer::posToOffset");
    }

    const void* Writer::write(const void* data, size_t length) {
        const void* result = _chunks.back().write(data, length);
        if (_usuallyFalse(!result))
            result = writeToNewChunk(data, length);
        _length += length;
        return result;
    }

    const void* Writer::writeToNewChunk(const void* data, size_t length) {
        if (_usuallyTrue(_chunkSize <= 64*1024))
            _chunkSize *= 2;
        addChunk(std::max(length, _chunkSize));
        const void *result = _chunks.back().write(data, length);
        assert(result);
        return result;
    }

    void Writer::rewrite(const void *pos, slice data) {
        assert(pos); //FIX: Check that it's actually inside a chunk
        ::memcpy((void*)pos, data.buf, data.size);
    }

    void Writer::addChunk(size_t capacity) {
        _chunks.emplace_back(capacity);
    }

    void Writer::freeChunk(Chunk &chunk) {
        if (chunk.start() != &_initialBuf)
            chunk.free();
    }

    std::vector<slice> Writer::output() const {
        std::vector<slice> result;
        result.reserve(_chunks.size());
        for (const Chunk &chunk : _chunks)
            result.push_back(chunk.contents());
        return result;
    }

    alloc_slice Writer::extractOutput() {
        alloc_slice output;
#if 0 //TODO: Restore this optimization
        if (_chunks.size() == 1 && _chunks[0].start() != &_initialBuf) {
            _chunks[0].resizeToFit();
            output = alloc_slice::adopt(_chunks[0].contents());
            _chunks.clear();
            _length = 0;
        } else
#endif
        {
            output = alloc_slice(length());
            void* dst = (void*)output.buf;
            for (auto &chunk : _chunks) {
                auto contents = chunk.contents();
                memcpy(dst, contents.buf, contents.size);
                dst = offsetby(dst, contents.size);
            }
            reset();
        }
        return output;
    }


#pragma mark - CHUNK:

    Writer::Chunk::Chunk(void *buf, size_t size)
    :_start(buf),
     _available(buf, size)
    { }

    Writer::Chunk::Chunk(size_t capacity)
    :_start(::malloc(capacity)),
    _available(_start, capacity)
    {
        if (_usuallyFalse(!_start))
            throw std::bad_alloc();
    }

    Writer::Chunk::Chunk(Chunk&& c) noexcept
    :_start(c._start),
     _available(c._available)
    {
        c._start = nullptr;
    }

    Writer::Chunk& Writer::Chunk::operator=(Chunk&& c) noexcept {
        _start = c._start;
        _available = c._available;
        c._start = nullptr;
        return *this;
    }


    void Writer::Chunk::free() noexcept {
        ::free(_start);
        _start = nullptr;
    }

    const void* Writer::Chunk::write(const void* data, size_t length) {
        if (_usuallyFalse(_available.size < length))
            return nullptr;
        const void *result = _available.buf;
        if (_usuallyTrue(data != nullptr))
            ::memcpy((void*)result, data, length);
        _available.moveStart(length);
        return result;
    }

    void Writer::Chunk::resizeToFit() noexcept {
        size_t len = length();
        void *newStart = ::realloc(_start, len);
        if (newStart)
            _start = newStart;
        _available = slice(offsetby(newStart,len), (size_t)0);
    }


#pragma mark - BASE64:


    void Writer::writeBase64(slice data) {
        size_t base64size = ((data.size + 2) / 3) * 4;
        auto dst = (char*)reserveSpace(base64size);
        base64::encoder enc;
        enc.set_chars_per_line(0);
        size_t written = enc.encode(data.buf, data.size, dst);
        written += enc.encode_end(dst + written);
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
