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
        for (auto &chunk : _chunks)
            chunk.free();
    }

    Writer& Writer::operator= (Writer&& w) {
        _chunks = w._chunks;
        w._chunks.resize(0);
        return *this;
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
        throw "invalid pos for posToOffset";
    }

    const void* Writer::write(const void* data, size_t length) {
        const void* result = _chunks.back().write(data, length);
        if (!result) {
            if (_chunkSize <= 64*1024)
                _chunkSize *= 2;
            addChunk(std::max(length, _chunkSize));
            result = _chunks.back().write(data, length);
            assert(result);
        }
        _length += length;
        return result;
    }

    void Writer::rewrite(const void *pos, slice data) {
        assert(pos); //FIX: Check that it's actually inside a chunk
        ::memcpy((void*)pos, data.buf, data.size);
    }

    void Writer::addChunk(size_t capacity) {
        auto chunk(capacity);
        _chunks.push_back(chunk);
    }

    alloc_slice Writer::extractOutput() {
        alloc_slice output;
        if (_chunks.size() == 1) {
            _chunks[0].resizeToFit();
            output = alloc_slice::adopt(_chunks[0].contents());
        } else {
            output = alloc_slice(length());
            void* dst = (void*)output.buf;
            for (auto &chunk : _chunks) {
                auto contents = chunk.contents();
                memcpy(dst, contents.buf, contents.size);
                dst = offsetby(dst, contents.size);
                chunk.free();
            }
        }
        _chunks.resize(0);
        _length = 0;
        return output;
    }


#pragma mark - CHUNK:

    Writer::Chunk::Chunk()
    :_start(NULL)
    { }

    Writer::Chunk::Chunk(size_t capacity)
    :_start(::malloc(capacity)),
    _available(_start, capacity)
    {
        if (!_start)
            throw std::bad_alloc();
    }

    Writer::Chunk::Chunk(const Chunk& c)
    :_start(c._start),
     _available(c._available)
    { }

    void Writer::Chunk::free() {
        ::free(_start);
        _start = NULL;
    }

    const void* Writer::Chunk::write(const void* data, size_t length) {
        if (_available.size < length)
            return NULL;
        const void *result = _available.buf;
        if (data != NULL)
            ::memcpy((void*)result, data, length);
        _available.moveStart(length);
        return result;
    }

    void Writer::Chunk::resizeToFit() {
        size_t len = length();
        void *newStart = ::realloc(_start, len);
        if (!newStart)
            throw std::bad_alloc();
        _start = newStart;
        _available = slice(offsetby(newStart,len), (size_t)0);
    }


#pragma mark - BASE64:


    /*
     base64.cpp and base64.h

     Copyright (C) 2004-2008 René Nyffenegger

     This source code is provided 'as-is', without any express or implied
     warranty. In no event will the author be held liable for any damages
     arising from the use of this software.

     Permission is granted to anyone to use this software for any purpose,
     including commercial applications, and to alter it and redistribute it
     freely, subject to the following restrictions:

     1. The origin of this source code must not be misrepresented; you must not
     claim that you wrote the original source code. If you use this source code
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.

     2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original source code.

     3. This notice may not be removed or altered from any source distribution.

     René Nyffenegger rene.nyffenegger@adp-gmbh.ch

     */

    // Modified to write to an ostream instead of returning a string, and to fix compiler warnings.

    static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

    static void base64_encode(Writer &out,
                              unsigned char const* bytes_to_encode,
                              unsigned int in_len)
    {
        int i = 0;
        int j = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];

        while (in_len--) {
            char_array_3[i++] = *(bytes_to_encode++);
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = (unsigned char)((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = (unsigned char)((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;

                for(i = 0; (i <4) ; i++)
                    out << base64_chars[char_array_4[i]];
                i = 0;
            }
        }

        if (i)
        {
            for(j = i; j < 3; j++)
                char_array_3[j] = '\0';

            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = (unsigned char)((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = (unsigned char)((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (j = 0; (j < i + 1); j++)
                out << base64_chars[char_array_4[j]];

            while((i++ < 3))
                out << '=';

        }
    }

    void Writer::writeBase64(slice data) {
        base64_encode(*this, (const unsigned char*)data.buf, (unsigned)data.size);
    }

}
