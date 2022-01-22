//
// FileUtils.hh
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "FleeceException.hh"
#include <stdio.h>

namespace fleece {

    static inline int checkErrno(int result, const char *msg) {
        if (_usuallyFalse(result < 0))
            FleeceException::_throwErrno("%s", msg);
        return result;
    }

    void check_fwrite(FILE *f, const void *data, size_t size);

    off_t check_getEOF(FILE*);

}
