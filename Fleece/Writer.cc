//
//  Writer.cc
//  Fleece
//
//  Created by Jens Alfke on 2/4/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "Writer.hh"
#include <assert.h>


namespace fleece {

    Writer::Writer(size_t initialCapacity)
    :_chunkSize(initialCapacity),
     _length(0)
    {
        addChunk(initialCapacity);
    }

    Writer::Writer(Writer&& w)
    :_chunks(w._chunks)
    {
        w._chunks.resize(0);
    }

    Writer::~Writer() {
        for (auto i = _chunks.begin(); i != _chunks.end(); ++i)
            (*i)->free();
    }

    Writer& Writer::operator= (Writer&& w) {
        _chunks = w._chunks;
        w._chunks.resize(0);
        return *this;
    }

    const void* Writer::curPos() const {
        return _chunks.back()->available().buf;
    }

    size_t Writer::posToOffset(const void *pos) const {
        size_t offset = 0;
        for (auto i = _chunks.begin(); i != _chunks.end(); ++i) {
            if ((*i)->contains(pos))
                return offset + (*i)->offsetOf(pos);
            offset += (*i)->length();
        }
        throw "invalid pos for posToOffset";
    }

    const void* Writer::write(const void* data, size_t length) {
        const void* result = _chunks.back()->write(data, length);
        if (!result) {
            if (_chunkSize <= 32*1024)
                _chunkSize *= 2;
            auto newChunk = addChunk(std::max(length, _chunkSize));
            result = newChunk->write(data, length);
        }
        _length += length;
        return result;
    }

    void Writer::rewrite(const void *pos, slice data) {
        assert(pos); //FIX: Check that it's actually inside a chunk
        ::memcpy((void*)pos, data.buf, data.size);
    }

    Writer::Chunk* Writer::addChunk(size_t capacity) {
        auto chunk = Chunk::create(capacity);
        _chunks.push_back(chunk);
        return chunk;
    }

    alloc_slice Writer::extractOutput() {
        alloc_slice output(length());
        void* dst = (void*)output.buf;
        for (auto i = _chunks.begin(); i != _chunks.end(); ++i) {
            auto contents = (*i)->contents();
            memcpy(dst, contents.buf, contents.size);
            dst = offsetby(dst, contents.size);
            (*i)->free();
        }
        _chunks.resize(0);
        _length = 0;
        return output;
    }

}
