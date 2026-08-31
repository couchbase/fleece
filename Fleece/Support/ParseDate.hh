//
//  ParseDate.h
//  Fleece
//
//  Copyright 2017-Present Couchbase, Inc.
//
//  Use of this software is governed by the Business Source License included
//  in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
//  in that file, in accordance with the Business Source License, use of this
//  software will be governed by the Apache License, Version 2.0, included in
//  the file licenses/APL2.txt.
//

#pragma once
#include "fleece/slice.hh"
#include <chrono>
#include <ctime>
#include <string>

#if defined(__linux__)
// Linux libstdc++ falls back to the hand-written formatter below; minimum Linux toolchain to
// drop this and make USE_STD_FORMAT unconditional: libstdc++ 13+ (e.g. Ubuntu 24.04 LTS's
// default toolchain). libstdc++ 12 and earlier don't have <format> at all; libstdc++ 13 has
// full std::format chrono support (%F/%T/%c/%A, including fractional seconds), verified with
// both g++-13 and clang-18.
#    define USE_STD_FORMAT 0
#else
#    define USE_STD_FORMAT 1
#    include <format>
#endif

namespace fleece {

#if !USE_STD_FORMAT
    /** Hand-written substitute for date::format/std::format's chrono formatter, covering exactly
        the specifiers this codebase actually uses (%F, %T, %Y, %m, %d, %H, %M, %S, %A, %c);
        everything else in `fmt` is copied through literally. Takes an already-decomposed
        calendar/time-of-day breakdown (see the format() overloads below) rather than a
        time_point directly, so this one non-template function serves every Duration without
        needing separate instantiations; defined in ParseDate.cc. */
    std::string formatChronoFields(const char* fmt, std::chrono::year_month_day date, std::chrono::weekday wd,
                                    long long hours, long long minutes, long long seconds, long long subseconds,
                                    int fractionalWidth);

    /** Decomposes any time_point (local_time or sys_time, any Duration) into calendar fields and
        calls formatChronoFields. Shared by both format() overloads below so the decomposition
        logic -- portable, no toolchain variance -- isn't duplicated between them. */
    template <class TimePoint>
    std::string formatViaFields(const char* fmt, TimePoint tp) {
        using namespace std::chrono;
        const auto           day  = floor<days>(tp);
        const year_month_day date{day};
        const hh_mm_ss       time{tp - day};
        const weekday        wd{day};
        return formatChronoFields(fmt, date, wd, time.hours().count(), time.minutes().count(), time.seconds().count(),
                                   decltype(time)::fractional_width > 0 ? time.subseconds().count() : 0,
                                   decltype(time)::fractional_width);
    }
#endif

    /** Formats a local or UTC time_point using a strftime-like format string (e.g. "%F", "%T",
        "%c", "%A"). This is the single seam for platform variance in std::format's chrono
        support: it calls std::format directly where available (see USE_STD_FORMAT above), and
        falls back to formatChronoFields (a small hand-written formatter covering just the
        specifiers this codebase uses elsewhere) otherwise. Callers don't need their own #if --
        the branch lives here, once.
        This overload (rather than one generic template accepting any type) exists deliberately:
        it constrains the accepted type to local_time, so misuse fails cleanly at the call site
        via overload resolution instead of deep inside a template body. */
    template <class Duration>
    std::string format(const char* fmt, std::chrono::local_time<Duration> tp) {
#if USE_STD_FORMAT
        return std::vformat(std::string("{:") + fmt + "}", std::make_format_args(tp));
#else
        return formatViaFields(fmt, tp);
#endif
    }

    /** Formats a UTC time_point using a strftime-like format string. See the local_time overload
        above (including why this is a separate overload rather than one generic template). */
    template <class Duration>
    std::string format(const char* fmt, std::chrono::sys_time<Duration> tp) {
#if USE_STD_FORMAT
        return std::vformat(std::string("{:") + fmt + "}", std::make_format_args(tp));
#else
        return formatViaFields(fmt, tp);
#endif
    }

    static constexpr int64_t kInvalidDate = INT64_MIN;

    typedef enum {
        kDateComponentMillennium,
        kDateComponentCentury,
        kDateComponentDecade,
        kDateComponentYear,
        kDateComponentQuarter,
        kDateComponentMonth,
        kDateComponentWeek,
        kDateComponentDay,
        kDateComponentHour,
        kDateComponentMinute,
        kDateComponentSecond,
        kDateComponentMillisecond,
        kDateComponentInvalid
    } DateComponent;

    /*
     ** A structure for holding a single date and time.
     */
    typedef struct DateTime DateTime;

    struct DateTime {
        int64_t iJD;       /* The julian day number times 86400000 */
        int     Y, M, D;   /* Year, month, and day */
        int     h, m;      /* Hour and minutes */
        int     tz;        /* Timezone offset in minutes */
        double  s;         /* Seconds */
        char    validYMD;  /* True (1) if Y,M,D are valid */
        char    validHMS;  /* True (1) if h,m,s are valid */
        char    validJD;   /* True (1) if iJD is valid */
        char    validTZ;   /* True (1) if tz is valid */
        char    separator; /* The character used to separate the date and time (T or space) */
    };

    /** Parses a C string as an ISO-8601 date-time, returning a parsed DateTime struct */
    DateTime ParseISO8601DateRaw(const char* dateStr);

    /** Parses a C string as an ISO-8601 date-time, returning a parsed DateTime struct */
    DateTime ParseISO8601DateRaw(slice dateStr);

    /** Converts an existing DateTime struct into a timestamp (milliseconds since 
         1/1/1970) */
    int64_t ToMillis(DateTime& dt, bool no_tz = false);

    /** Converts a timestamp (milliseconds since 1/1/1970) into a parsed DateTime struct
        in UTC time */
    DateTime FromMillis(int64_t timestamp);

    /** Parses a C string as an ISO-8601 date-time, returning a timestamp (milliseconds since
        1/1/1970), or kInvalidDate if the string is not valid. */
    int64_t ParseISO8601Date(const char* dateStr);

    /** Parses a C string as an ISO-8601 date-time, returning a timestamp (milliseconds since
        1/1/1970), or kInvalidDate if the string is not valid. */
    int64_t ParseISO8601Date(slice dateStr);

    /** Parses a C string as a date component (valid strings are represented by the DateComponent
        enum above) */
    DateComponent ParseDateComponent(slice component);

    /** Maximum length of a formatted ISO-8601 date. (Actually it's a bit bigger.) */
    static constexpr size_t kFormattedISO8601DateMaxSize = 40;

    /** Formats a timestamp (milliseconds since 1/1/1970) as an ISO-8601 date-time.
        @param buf  The location to write the formatted C string. At least
                    kFormattedISO8601DateMaxSize bytes must be available.
        @param timestamp  The timestamp (milliseconds since 1/1/1970).
        @param asUTC  True to format as UTC, false to use the local time-zone.
        @param format The model to use for formatting (i.e. which portions to include).
                      If null, then the full ISO-8601 format is used
        @return  The formatted string (points to `buf`). */
    slice FormatISO8601Date(char* buf LIFETIMEBOUND, int64_t timestamp, bool asUTC, const DateTime* format);

    /** Formats a timestamp (milliseconds since 1/1/1970) as an ISO-8601 date-time.
        @param buf  The location to write the formatted C string. At least
                    kFormattedISO8601DateMaxSize bytes must be available.
        @param timestamp  The timestamp (milliseconds since 1/1/1970).
        @param tzoffset   The timezone offset from UTC in minutes
        @param format The model to use for formatting (i.e. which portions to include).
                      If null, then the full ISO-8601 format is used
        @return  The formatted string (points to `buf`). */
    slice FormatISO8601Date(char* buf LIFETIMEBOUND, int64_t timestamp, std::chrono::minutes tzoffset, const DateTime* format);

    /** Creates a tm out of a timestamp, but it will not be fully valid until
        passed through mktime.
        @param timestamp  The timestamp to use
        @return  The populated tm struct (dst value will be unset)
    */
    struct tm FromTimestamp(std::chrono::seconds timestamp);

    /** Calculates the timezone offset from UTC given a reference date.
        This function does its best to be daylight savings time aware.
        Note that some platforms (notably Windows) cannot handle dates
        before the epoch.  In these cases, DST is disregarded.
        @param time  The time to calculate the time zone offset for
        @param input_utc  If true, the input time is in UTC, and will be
                          Converted to local time before considering DST
        @return  The time elapsed since 1/1/1970 as a duration
    */
    std::chrono::seconds GetLocalTZOffset(struct tm* time, bool input_utc);
}  // namespace fleece
