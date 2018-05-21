//
// FileUtils.cc
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

#include "FileUtils.hh"

namespace fleece {


    void check_fwrite(FILE *f, const void *data, size_t size) {
        auto written = fwrite(data, size, 1, f);
        if (written < 1)
            FleeceException::_throwErrno("Can't write to file");
    }

    off_t check_getEOF(FILE *f) {
        checkErrno(fseek(f, 0, SEEK_END), "Can't get file size");
        off_t eof = ftello(f);
        if (eof < 0)
            FleeceException::_throwErrno("Can't get file size");
        return eof;
    }
}
