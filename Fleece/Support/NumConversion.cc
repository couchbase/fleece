//
// NumConversion.cc
//
// Copyright © 2019 Couchbase. All rights reserved.
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

#include "NumConversion.hh"
#include "SwiftDtoa.h"
#include <locale.h>
#include <stdlib.h>
#ifndef _MSC_VER
#include <xlocale.h>
#endif

namespace fleece {

    double ParseDouble(const char *str) noexcept {
        // strtod is locale-aware, so in some locales it will not interpret '.' as a decimal point.
        // To work around that, use the C locale explicitly.
        #ifdef LC_C_LOCALE          // Apple & BSD
            return strtod_l(str, nullptr, LC_C_LOCALE);
        #elif defined(_MSC_VER)     // Windows
            static _locale_t kCLocale = _create_locale(LC_ALL, "C");
            return _strtod_l(str, nullptr, kCLocale);
        #elif defined(__ANDROID__) && __ANDROID_API__ < 26
            // Note: Android only supports the following locales, all of which use
            // period, so no problem:  C, POSIX, en_US.  Android API 26 introduces
            // strtod_l, which maybe will be eventually implemented when and if more
            // locales comin
            return strtod(str);
        #else                       // Linux
            static locale_t kCLocale = newlocale(LC_ALL_MASK, "C", NULL);
            return strtod_l(str, nullptr, kCLocale);
        #endif
    }


    size_t WriteFloat(float n, char *dst, size_t capacity) {
        return swift_format_float(n, dst, capacity);
    }


    size_t WriteFloat(double n, char *dst, size_t capacity) {
        return swift_format_double(n, dst, capacity);
    }
}
