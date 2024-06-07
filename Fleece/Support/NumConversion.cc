//
// NumConversion.cc
//
// Copyright 2019-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
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


    static bool onlyWhitespace(const char *str) {
        while (isspace(*str))
            ++str;
        return *str == '\0';
    }

    // subroutine that parses only digits
    static bool _parseUInt(const char* FL_NONNULL str, uint64_t &result, bool allowTrailing) {
        uint64_t n = 0;
        if (!isdigit(*str))
            return false;
        while (isdigit(*str)) {
            int digit = (*str++ - '0');
            if (_usuallyFalse(n > UINT64_MAX / 10))
                return false;
            n *= 10;
            if (_usuallyFalse(n > UINT64_MAX - digit))
                return false;
            n += digit;
        }
        if (!allowTrailing && !onlyWhitespace(str))
            return false;
        result = n;
        return true;
    }

    // Unsigned version:
    bool ParseInteger(const char* FL_NONNULL str, uint64_t &result, bool allowTrailing) {
        while (isspace(*str))
            ++str;
        if (*str == '+')
            ++str;
        return _parseUInt(str, result, allowTrailing);
    }


    // Signed version:
    bool ParseInteger(const char* FL_NONNULL str, int64_t &result, bool allowTrailing) {
        while (isspace(*str))
            ++str;
        bool negative = (*str == '-');
        if (negative || *str == '+')
            ++str;
        uint64_t uresult;
        if (!_parseUInt(str, uresult, allowTrailing))
            return false;

        if (negative) {
            if (_usuallyTrue(uresult <= uint64_t(INT64_MAX))) {
                result = -int64_t(uresult);
            } else if (uresult == uint64_t(INT64_MAX) + 1) {
                // Special-case the conversion of 9223372036854775808 into -9223372036854775808,
                // because the normal cast (above) would create a temporary integer overflow that
                // triggers a runtime Undefined Behavior Sanitizer warning.
                result = INT64_MIN;
            } else {
                return false;
            }
        } else {
            if (_usuallyFalse(uresult > uint64_t(INT64_MAX)))
                return false;
            result = int64_t(uresult);
        }
        return true;
    }


    double ParseDouble(const char *str) noexcept {
        double result;
        (void)ParseDouble(str, result, true);
        return result;
    }


    bool ParseDouble(const char* FL_NONNULL str, double &result, bool allowTrailing) {
        char *endptr;
        // strtod is locale-aware, so in some locales it will not interpret '.' as a decimal point.
        // To work around that, use the C locale explicitly.
        #ifdef LC_C_LOCALE          // Apple & BSD
            result = strtod_l(str, &endptr, LC_C_LOCALE);
        #elif defined(_MSC_VER)     // Windows
            static _locale_t kCLocale = _create_locale(LC_ALL, "C");
            result = _strtod_l(str, &endptr, kCLocale);
        #elif defined(__ANDROID__) && __ANDROID_API__ < 26
            // Note: Android only supports the following locales, all of which use
            // period, so no problem:  C, POSIX, en_US.  Android API 26 introduces
            // strtod_l, which maybe will be eventually implemented when and if more
            // locales come in
            result = strtod(str, &endptr);
        #else                       // Linux
            static locale_t kCLocale = newlocale(LC_ALL_MASK, "C", NULL);
            result = strtod_l(str, &endptr, kCLocale);
        #endif
        return endptr > str && (allowTrailing || onlyWhitespace(endptr));
    }


    size_t WriteFloat(float n, char *dst, size_t capacity) {
        return swift_format_float(n, dst, capacity);
    }


    size_t WriteFloat(double n, char *dst, size_t capacity) {
        return swift_format_double(n, dst, capacity);
    }
}
