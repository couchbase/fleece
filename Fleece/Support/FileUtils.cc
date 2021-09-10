//
// FileUtils.cc
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "FileUtils.hh"
#ifdef _MSC_VER
#define ftello      ftell
#endif

namespace fleece {


    void check_fwrite(FILE *f, const void *data, size_t size) {
        auto written = fwrite(data, 1, size, f);
        if (written < size)
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
