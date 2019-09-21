//
// PlatformCompat.cc
//
// Copyright (c) 2019 Couchbase, Inc All rights reserved.
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

#include "PlatformCompat.hh"
#include <clocale>
#include <mutex>
#include <locale>
#include <cstdarg>
#ifdef __APPLE__
#include <xlocale.h>
#endif
using namespace std;

#ifdef _MSC_VER
#define BEGIN_LOCALE() _configthreadlocale(_ENABLE_PER_THREAD_LOCALE); \
    const char* __l = setlocale(LC_ALL, "C")

#define RETURN_END_LOCALE(name) setlocale(LC_ALL, __l); \
    _configthreadlocale(_DISABLE_PER_THREAD_LOCALE); \
    return name

#else
#ifndef __APPLE__
locale_t _c_locale = newlocale(LC_ALL_MASK, "C", nullptr);
#endif
#define BEGIN_LOCALE() locale_t __l = uselocale(_c_locale);

#define RETURN_END_LOCALE(name) uselocale(__l); \
    return name

#endif

int cbl_sprintf_l(char* buf, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    BEGIN_LOCALE();
    const int result = vsprintf(buf, fmt, args);
    va_end(args);
    RETURN_END_LOCALE(result);
}

double cbl_strtod_l(const char* start, char** end) {
    BEGIN_LOCALE();
    const double result = strtod(start, end);
    RETURN_END_LOCALE(result);
}
