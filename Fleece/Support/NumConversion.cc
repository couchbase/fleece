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
#include <ctype.h>
#include <locale.h>
#include <stdlib.h>
#if !defined(_MSC_VER) && !defined(__GLIBC__)
#include <xlocale.h>
#endif

namespace fleece {

    // subroutine that parses only digits
    static bool _parseUInt(const char *str NONNULL, uint64_t &result, bool allowTrailing) {
        uint64_t n = 0;
        if (!isdigit(*str))
            return false;
        while (isdigit(*str)) {
            int digit = (*str++ - '0');
            if (n > UINT64_MAX / 10)
                return false;
            n *= 10;
            if (n > UINT64_MAX - digit)
                return false;
            n += digit;
        }
        if (!allowTrailing) {
            while (isspace(*str))
                ++str;
            if (*str != '\0')
                return false;
        }
        result = n;
        return true;
    }

    // Unsigned version:
    bool ParseInteger(const char *str NONNULL, uint64_t &result, bool allowTrailing) {
        while (isspace(*str))
            ++str;
        if (*str == '+')
            ++str;
        return _parseUInt(str, result, allowTrailing);
    }


    // Signed version:
    bool ParseInteger(const char *str NONNULL, int64_t &result, bool allowTrailing) {
        while (isspace(*str))
            ++str;
        bool negative = (*str == '-');
        if (negative || *str == '+')
            ++str;
        uint64_t uresult;
        if (!_parseUInt(str, uresult, allowTrailing))
            return false;
        if (uresult > uint64_t(INT64_MAX) + negative)
            return false;
        result = negative ? int64_t(-uresult) : int64_t(uresult);
        return true;
    }


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
            // locales come in
            return strtod(str, nullptr);
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
