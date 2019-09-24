//
// NumConversion.hh
//
// Copyright © 2019 Couchbase. All rights reserved.
//

#pragma once
#include "PlatformCompat.hh"
#include <stddef.h>

namespace fleece {

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
