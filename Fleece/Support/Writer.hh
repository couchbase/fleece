//
// Writer.hh
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

#pragma once

#include "fleece/slice.hh"
#include "SmallVector.hh"
#include <stdio.h>
#include <vector>

namespace fleece {

    /** A simple write-only stream that buffers its output into a slice.
        (Used instead of C++ ostreams because those have too much overhead.) */
    class Writer {
    public:
        static const size_t kDefaultInitialCapacity = 256;

        Writer(size_t initialCapacity =kDefaultInitialCapacity);
        Writer(FILE * NONNULL outputFile);
        ~Writer();

        Writer(Writer&&) noexcept;
        Writer& operator= (Writer&&) noexcept;

        void reset();

        size_t length() const                   {return _baseOffset + _length;}
        const void* curPos() const;
        FILE* outputFile() const                {return _outputFile;}

        /** Returns the data written, in pieces. Does not change the state of the Writer. */
        std::vector<slice> output() const;

        /** Returns the data written. The Writer stops managing this memory; it now belongs to
            the caller and will be freed when no more alloc_slices refer to it. */
        alloc_slice finish();

        /** Writes data. If the output is going to memory (the default), returns a pointer to where
            the data got written to. If the output is being written to a file, returns nullptr. */
        const void* write(const void* data, size_t length);

        const void* write(slice s)              {return write(s.buf, s.size);}

        Writer& operator<< (uint8_t byte)       {return operator<<(slice(&byte,1));}
        Writer& operator<< (slice s)            {write(s); return *this;}

        void writeBase64(slice data);
        void writeDecodedBase64(slice base64String);

        /** Pads output to even length by writing a zero byte if necessary. */
        void padToEvenLength();

        /** Reserves space for data without actually writing anything yet.
            The data must be written later using rewrite() otherwise there will be garbage in
            the output. */
        const void* reserveSpace(size_t length)      {return write(nullptr, length);}

        bool writeOutputToFile(FILE *);

    private:
        class Chunk {
        public:
            Chunk(size_t capacity);
            Chunk(void *buf, size_t size);
            Chunk(Chunk&&) noexcept;
            Chunk(const Chunk&) =delete;
            Chunk& operator=(Chunk&&) noexcept;
            void free() noexcept;
            void reset()              {_available.setStart(_start);}
            const void* write(const void* data, size_t length);
            bool pad();
            void resizeToFit() noexcept;
            void* start()             {return _start;}
            size_t length() const     {return (int8_t*)_available.buf - (int8_t*)_start;}
            size_t capacity() const   {return (int8_t*)_available.end() - (int8_t*)_start;}
            slice contents() const    {return slice(_start, _available.buf);}
            slice available() const   {return _available;}
            bool contains(const void *ptr) const   {return ptr >= _start && ptr <= _available.buf;}
            size_t offsetOf(const void *ptr) const {return (int8_t*)ptr - (int8_t*)_start;}
        private:
            void *_start;
            slice _available;
        };

        void _reset();
        const void* writeToNewChunk(const void* data, size_t length);
        void addChunk(size_t capacity);
        void freeChunk(Chunk &chunk);

        Writer(const Writer&) = delete;
        const Writer& operator=(const Writer&) = delete;

        smallVector<Chunk, 4> _chunks;
        size_t _chunkSize;
        size_t _baseOffset {0};
        size_t _length {0};
        uint8_t _initialBuf[kDefaultInitialCapacity];
        FILE* _outputFile;
    };

}
