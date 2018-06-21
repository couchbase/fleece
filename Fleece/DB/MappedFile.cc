//
// MappedFile.cc
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

#include "MappedFile.hh"
#include "FleeceException.hh"
#include <errno.h>
#include <stdexcept>

namespace fleece {
    using namespace std;


    MappedFile::MappedFile(const char *path, const char *mode, size_t maxSize)
    :_path(path)
    ,_mode(mode)
    ,_maxSize(maxSize)
    {
        open();
    }


    MappedFile::~MappedFile() {
        close();
    }


    void MappedFile::open() {
        if (_fd)
            return;

#ifdef FL_ESP32
        _mapping = esp_mapped_slice(_path.c_str());
        _fd = _mapping.open(_mode.c_str(), 32768);
#else
        if (_mode == "rw+") {
            // "rw+" means to open read/write, create the file if necessary, but not truncate it:
            _fd = fopen(_path.c_str(), "r+");
            if (!_fd && errno == ENOENT)
                _fd = fopen(_path.c_str(), "w+");
        } else {
            _fd = fopen(_path.c_str(), _mode.c_str());
        }
#endif

        if (!_fd)
            FleeceException::_throwErrno("Can't open file");

        off_t fileSize = getFileSize();
#ifdef FL_ESP32
        _maxSize = _mapping.size;
#else
        if (_maxSize == 0)
            _maxSize = (size_t)min(fileSize, (off_t)SIZE_MAX);
        _mapping = mmap_slice(_fd, _maxSize);
#endif

        resizeTo(fileSize);
    }


    void MappedFile::resizeTo(off_t size) {
        assert(size >= 0);
        if ((uint64_t)size > (uint64_t)_maxSize)
            throw runtime_error("MappedFile isn't large enough to hold file");
        _contents = slice(_mapping.buf, (size_t)size);
    }


    off_t MappedFile::getFileSize() {
        if (!_fd)
            throw logic_error("MappedFile is not open");
        if (fseeko(_fd, 0, SEEK_END) >= 0) {
            auto size = ftello(_fd);
            if (size >= 0)
                return size;
        }
        FleeceException::_throwErrno("Can't get the file's length");
    }


    void MappedFile::close() {
        _mapping.unmap();
        _contents = nullslice;
        if (_fd) {
            fclose(_fd);
            _fd = nullptr;
        }
    }

}
