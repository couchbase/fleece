//
// MappedFile.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "RefCounted.hh"
#include "sliceIO.hh"
#include <string>
#include <stdio.h>

namespace fleece {

    /** Memory-maps a file and exposes its contents as a slice */
    class MappedFile : public RefCounted {
    public:
        /** Constructs a MappedFile.
            @param path  The path to the file.
            @param mode  The mode, as given to `fopen`. As an extension, the value "rw+" opens
                        the file for read-write access and creates it if it doesn't exist, but
                        doesn't truncate it.
            @param maxSize  The amount of address space to allocate; this must be no smaller than
                        the size the file will become while open. If zero, the file's current
                        size will be used instead. */
        explicit MappedFile(const char *path, const char *mode = "r", size_t maxSize =0);

        const char *path() const                                {return _path.c_str();}
        
        /** The current file contents. This will update if existing data in the file is overwritten,
            but it will not grow (or shrink) if the file's EOF changes. For that you need to call
            `resizeToEOF`. */
        slice contents() const                                  {return _contents;}

        /** The open file handle. Don't close it!! */
        FILE* fileHandle() const                                {return _fd;}

        /** Changes the size of `contents()` to match the file's current EOF.
            The base address (`buf`) does not change. */
        void resizeToEOF()                                      {resizeTo(getFileSize());}

        /** Changes the size of `contents`. Use with caution. */
        void resizeTo(off_t size);

        /** Closes the file, if you need it to close before the MappedFile is destructed. */
        void close();

        /** Reopens the file after it's been closed. Otherwise it's a no-op. */
        void open();

    protected:
        ~MappedFile();
    private:
        MappedFile(const MappedFile&) =delete;
        off_t getFileSize();

        std::string const _path, _mode;
        size_t _maxSize;
        FILE* _fd {nullptr};
        mmap_slice _mapping;
        slice _contents;
    };


}
