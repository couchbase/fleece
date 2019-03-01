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

        size_t length() const                   {return _length - _available.size;}
        const void* curPos() const              {return _available.buf;}
        FILE* outputFile() const                {return _outputFile;}

        void flush();

        /** Invokes the callback for each range of bytes in the output. */
        template <class T>
        void forEachChunk(T callback) const {
            assert(!_outputFile);
            auto n = _chunks.size();
            for (auto chunk : _chunks) {
                if (_usuallyFalse(--n == 0)) {
                    chunk.setSize(chunk.size - _available.size);
                    if (chunk.size == 0)
                        continue;
                }
                callback(chunk);
            }
        }

        /** Returns the data written, in pieces. Does not change the state of the Writer. */
        std::vector<slice> output() const;

        /** Returns the data written. The Writer stops managing this memory; it now belongs to
            the caller and will be freed when no more alloc_slices refer to it. */
        alloc_slice finish();

        /** Writes data. Returns a pointer to where the data got written to. */
        const void* write(slice s) {
            const void* result;
            if (_usuallyTrue(s.size <= _available.size)) {
                result = _available.buf;
                if (s.buf)
                    ::memcpy((void*)_available.buf, s.buf, s.size);
                _available.moveStart(s.size);
            } else {
                result = writeToNewChunk(s);
            }
            // No need to add to _length; the length() method compensates.
            assertLengthCorrect();
            return result;
        }

        const void* write(const void* data NONNULL, size_t length)  {return write({data, length});}

        /** Reserves space for data without actually writing anything yet.
            The data must be filled in later, otherwise there will be garbage in
            the output. */
        void* reserveSpace(size_t length) {
            const void* result;
            if (_usuallyTrue(length <= _available.size)) {
                result = _available.buf;
                _available.moveStart(length);
            } else {
                result = writeToNewChunk({nullptr, length});
            }
            // No need to add to _length; the length() method compensates.
            assertLengthCorrect();
            return (void*) result;
        }

        /** Reserves space for \ref count values of type \ref T. */
        template <class T>
        T* reserveSpace(size_t count)           {return (T*) reserveSpace(count * sizeof(T));}

        Writer& operator<< (uint8_t byte)       {return operator<<(slice(&byte,1));}
        Writer& operator<< (slice s)            {write(s); return *this;}

        /** Pads output to even length by writing a zero byte if necessary. */
        void padToEvenLength()                  {if (length() & 1) *this << 0;}

        void writeBase64(slice data);
        void writeDecodedBase64(slice base64String);

        bool writeOutputToFile(FILE *);

    private:
        void _reset();
        const void* writeToNewChunk(slice);
        void addChunk(size_t capacity);
        void freeChunk(slice);

        Writer(const Writer&) = delete;
        const Writer& operator=(const Writer&) = delete;

#if DEBUG
        void assertLengthCorrect() const;
#else
        void assertLengthCorrect() const { }
#endif

        slice _available;               // Available range of current chunk
        smallVector<slice, 4> _chunks;  // Chunks in consecutive order. Last is written to.
        size_t _chunkSize;              // Size of next chunk to allocate
        size_t _length {0};             // Output length, offset by _available.size
        FILE* _outputFile;              // File writing to, or NULL
        uint8_t _initialBuf[kDefaultInitialCapacity];   // Inline buffer to avoid a malloc
    };

}
