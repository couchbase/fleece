//
// NumConversion.hh
//
// Copyright © 2019 Couchbase. All rights reserved.
//

#pragma once
#include "PlatformCompat.hh"
#include <stddef.h>
#include <stdint.h>

namespace fleece {

    /// Parse `str` as an integer, storing the result in `result` and returning true.
    /// Returns false if the string is not a valid integer, or if the result is too large
    /// to fit in an `int64_t`.
    /// Expected: optional whitespace, an optional '-' or '+',  one or more decimal digits.
    /// If `allowTrailing` is false it also rejects anything but whitespace after the last digit.
    bool ParseInteger(const char *str NONNULL, int64_t &result, bool allowTrailing =false);

    /// Parse `str` as an unsigned integer, storing the result in `result` and returning true.
    /// Returns false if the string is not a valid unsigned integer, or if the result is too large
    /// to fit in a `uint64_t`.
    /// Expected: optional whitespace, an optional '+', one or more decimal digits.
    /// If `allowTrailing` is false it also rejects anything but whitespace after the last digit.
    bool ParseInteger(const char *str NONNULL, uint64_t &result, bool allowTrailing =false);

    /// Alternative syntax for parsing an unsigned integer.
    static inline bool ParseUnsignedInteger(const char *str NONNULL, uint64_t &r, bool t =false) {
        return ParseInteger(str, r, t);
    }

    
    /// Parse `str` as a floating-point number, reading as many digits as possible.
    /// (I.e. non-numeric characters after the digits are not treated as an error.)
    double ParseDouble(const char *str NONNULL) noexcept;


    /// Format a 64-bit-floating point number to a string.
    size_t WriteFloat(double n, char *dst, size_t capacity);

    /// Format a 32-bit floating-point number to a string.
    size_t WriteFloat(float n, char *dst, size_t capacity);

    /// Alternative syntax for formatting a 64-bit-floating point number to a string.
    static inline size_t WriteDouble(double n, char *dst, size_t c)  {return WriteFloat(n, dst, c);}

}
