//
//  Writer.hh
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

#ifndef Fleece_Writer_hh
#define Fleece_Writer_hh

#include "slice.hh"
#include <vector>

namespace fleece {

    /** A simple write-only stream that buffers its output into a slice.
        (Used instead of C++ ostreams because those have too much overhead.) */
    class Writer {
    public:
        static const size_t kDefaultInitialCapacity = 256;

        Writer(size_t initialCapacity =kDefaultInitialCapacity);
        Writer(Writer&&);
        ~Writer();

        Writer& operator= (Writer&&);

        size_t length() const                   {return _length;}
        const void* curPos() const;
        size_t posToOffset(const void *pos) const;

        /** Returns the data written. The Writer stops managing the memory; it is the caller's
            responsibility to free the returned slice's buf! */
        alloc_slice extractOutput();

        const void* write(const void* data, size_t length);

        Writer& operator<< (uint8_t byte)       {return operator<<(slice(&byte,1));}
        Writer& operator<< (slice s)            {write(s.buf, s.size); return *this;}

        /** Reserves space for data without actually writing anything yet.
            The data must be written later using rewrite() otherwise there will be garbage in
            the output. */
        const void* reserveSpace(size_t length)      {return write(NULL, length);}

        /** Overwrites already-written data.
            @param pos  The position in the output at which to start overwriting
            @param newData  The data that replaces the old */
        void rewrite(const void *pos, slice newData);

    private:
        class Chunk {
        public:
            static Chunk* create(size_t capacity) {
                auto chunk = (Chunk*)::malloc(sizeof(Chunk) + capacity);
                if (!chunk)
                    throw std::bad_alloc();
                chunk->_start = chunk+1;
                chunk->_available = slice(chunk->_start, capacity);
                return chunk;
            }

            void free() {
                ::free(this);
            }

            const void* write(const void* data, size_t length) {
                if (_available.size < length)
                    return NULL;
                const void *result = _available.buf;
                if (data != NULL)
                    ::memcpy((void*)result, data, length);
                _available.moveStart(length);
                return result;
            }

            size_t length()     {return (int8_t*)_available.buf - (int8_t*)_start;}
            size_t capacity()   {return (int8_t*)_available.end() - (int8_t*)_start;}
            slice contents()    {return slice(_start, _available.buf);}
            slice available()   {return _available;}

            bool contains(const void *ptr)      {return ptr >= _start && ptr <= _available.buf;}
            size_t offsetOf(const void *ptr)    {return (int8_t*)ptr - (int8_t*)_start;}
            
        private:
            void *_start;
            slice _available;
        };

        Chunk* addChunk(size_t capacity);
        Writer(const Writer&);  // forbidden
        const Writer& operator=(const Writer&); // forbidden

        std::vector<Chunk*> _chunks;
        size_t _chunkSize;
        size_t _length;
    };

}

#endif /* Fleece_Writer_hh */
