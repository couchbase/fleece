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
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#include "asprintf.h"
#endif

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
    static const char* log(const char *format, const char *cond, const char *fn,
                           const char *file, int line)
    {
        char *msg;
        if (asprintf(&msg, format, cond, (fn ? fn : ""), filename(file), line) > 0) {
            fprintf(stderr, "%s\n", msg);
            // (Yes, this leaks 'msg'. Under the circumstances, not an issue.)
            return msg;
        } else {
            // Best we can do if even malloc has failed us:
            fprintf(stderr, "%s\n", format);
            return format;
        }
    }


#ifdef __cpp_exceptions
    __cold
    void _assert_failed(const char *cond, const char *fn, const char *file, int line) {
        throw assertion_failure(
                    log("FAILED ASSERTION `%s` in %s (at %s line %d)",
                        cond, fn, file, line));
    }

    __cold
    void _precondition_failed(const char *cond, const char *fn, const char *file, int line) {
        throw std::invalid_argument(
                    log("FAILED PRECONDITION: `%s` not true when calling %s (at %s line %d)",
                        cond, fn, file, line));
    }

    __cold
    void _postcondition_failed(const char *cond, const char *fn, const char *file, int line) {
        throw assertion_failure(
                    log("FAILED POSTCONDITION: `%s` not true at end of %s (at %s line %d)",
                        cond, fn, file, line));
    }


    // These won't have had prototypes yet, since exceptions were enabled when parsing the header:
    [[noreturn]] NOINLINE void _assert_failed_nox(const char *condition, const char *fn,
                                                  const char *file, int line);
    [[noreturn]] NOINLINE void _precondition_failed_nox(const char *condition, const char *fn,
                                                        const char *file, int line);
    [[noreturn]] NOINLINE void _postcondition_failed_nox(const char *condition, const char *fn,
                                                         const char *file, int line);
#endif

    
    // Variants used when exceptions are disabled at the call site.

    __cold
    void _assert_failed_nox(const char *cond, const char *fn, const char *file, int line) {
        log("\n***FATAL: FAILED ASSERTION `%s` in %s (at %s line %d)",
                   cond, fn, file, line);
        std::terminate();
    }

    __cold
    void _precondition_failed_nox(const char *cond, const char *fn, const char *file, int line) {
        log("\n***FATAL: FAILED PRECONDITION: `%s` not true when calling %s (at %s line %d)",
                   cond, fn, file, line);
        std::terminate();
    }

    __cold
    void _postcondition_failed_nox(const char *cond, const char *fn, const char *file, int line) {
        log("***FATAL: FAILED POSTCONDITION: `%s` not true at end of %s (at %s line %d)",
                   cond, fn, file, line);
        std::terminate();
    }

}
