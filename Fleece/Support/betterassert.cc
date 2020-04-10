//
// betterassert.cc
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

#include "betterassert.hh"
#include <stdexcept>
#include <stdio.h>
#include <string.h>

namespace fleece {

    __cold
    static const char* filename(const char *file) {
#ifndef __FILE_NAME__
        const char *slash = strrchr(file, '/');
        if (!slash)
            slash = strrchr(file, '\\');
        if (slash)
            file = slash + 1;
#endif
        return file;
    }

    __cold
    void _assert_failed(const char *condition, const char *fn, const char *file, int line) {
        char *msg;
        asprintf(&msg, "FAILED ASSERTION `%s` in %s (at %s line %d)",
                condition, (fn ? fn : ""), filename(file), line);
        fprintf(stderr, "%s\n", msg);
        throw assertion_failure(msg);
    }

    __cold
    void _precondition_failed(const char *condition, const char *fn, const char *file, int line) {
        char *msg;
        asprintf(&msg, "FAILED PRECONDITION: `%s` not true when calling %s (at %s line %d)",
                condition, (fn ? fn : "?"), filename(file), line);
        fprintf(stderr, "%s\n", msg);
        throw std::invalid_argument(msg);
    }

    __cold
    void _postcondition_failed(const char *condition, const char *fn, const char *file, int line) {
        char *msg;
        asprintf(&msg, "FAILED POSTCONDITION: `%s` not true at end of %s (at %s line %d)",
                (fn ? fn : "?"), condition, filename(file), line);
        fprintf(stderr, "%s\n", msg);
        throw assertion_failure(msg);
    }

}
