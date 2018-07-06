//
// FileUtils.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "FleeceException.hh"
#include <stdio.h>

namespace fleece {

    static inline int checkErrno(int result, const char *msg) {
        if (_usuallyFalse(result < 0))
            FleeceException::_throwErrno(msg);
        return result;
    }

    void check_fwrite(FILE *f, const void *data, size_t size);

    off_t check_getEOF(FILE*);

}
