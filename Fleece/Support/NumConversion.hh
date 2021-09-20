//
// NumConversion.hh
//
// Copyright 2019-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "PlatformCompat.hh"

#if __has_include("Error.hh")
#include "Error.hh"
#else
#include "FleeceException.hh"
#endif

#include <cstddef>
#include <cstdint>
#include <cinttypes>
#include <typeinfo>
#include <limits>

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

    #if DEBUG
        template<typename Out, typename In>
        static Out narrow_cast (In val) {
            static_assert(::std::is_arithmetic<In>::value && ::std::is_arithmetic<Out>::value, "Only numeric types are valid for narrow_cast");
            if(sizeof(In) <= sizeof(Out) && ::std::is_signed<In>::value == ::std::is_signed<Out>::value) {
                return (Out)val;
            }

            // Comparing an unsigned number against a signed minimum causes issues, at least on Windows,
            // but if the output is signed and the input is unsigned then there is never a case where
            // the input could underflow the output.  The reverse situation does not appear to be true.
            // the input could underflow the output.  The reverse situation does not appear to be true.
            bool min_ok = std::is_signed<Out>::value && !std::is_signed<In>::value;

#ifdef Assert
            if(::std::is_floating_point<In>::value) {
                Assert(val >= std::numeric_limits<Out>::min() && val <= ::std::numeric_limits<Out>::max(), 
                    "Invalid narrow_cast %g -> %s", (double)val, typeid(Out).name());
            } else if(::std::is_signed<In>::value) {
                Assert((min_ok || val >= std::numeric_limits<Out>::min()) && val <= ::std::numeric_limits<Out>::max(), 
                    "Invalid narrow_cast %" PRIi64 " -> %s", (int64_t)val, typeid(Out).name());
            } else {
                Assert((min_ok || val >= std::numeric_limits<Out>::min()) && val <= ::std::numeric_limits<Out>::max(), 
                    "Invalid narrow_cast %" PRIu64 " -> %s", (uint64_t)val, typeid(Out).name());
            }
#else
            char message[256];
            if(::std::is_floating_point<In>::value) {
                sprintf(message, "Invalid narrow_cast %g -> %s", (double)val, typeid(Out).name());
                throwIf(val < std::numeric_limits<Out>::min() || val > std::numeric_limits<Out>::max(), InternalError, message);
            } else if(::std::is_signed<In>::value) {
                sprintf(message, "Invalid narrow_cast %" PRIi64 " -> %s", (int64_t)val, typeid(Out).name());
                throwIf((!min_ok && val < std::numeric_limits<Out>::min()) || val > std::numeric_limits<Out>::max(),
                    InternalError, message);
            } else {
                sprintf(message, "Invalid narrow_cast %" PRIu64 " -> %s", (uint64_t)val, typeid(Out).name());
                throwIf((!min_ok && val < std::numeric_limits<Out>::min()) || val > std::numeric_limits<Out>::max(),
                    InternalError, message);
            }
#endif
            return (Out)val;
        }
    #else
        template<typename Out, typename In>
        static inline Out narrow_cast(In val) {
            return static_cast<Out>(val);
        }
    #endif

}
