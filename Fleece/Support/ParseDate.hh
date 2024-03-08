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

namespace fleece {
    using namespace std::chrono;

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

    /** Creates a tm out of a timestamp, but it will not be fully valid until
        passed through mktime.
        @param timestamp  The timestamp to use
        @return  The populated tm struct (dst value will be unset)
    */
    struct tm FromTimestamp(seconds timestamp);

    /** Calculates the timezone offset from UTC given a reference date.
        This function does its best to be daylight savings time aware.
        Note that some platforms (notably Windows) cannot handle dates
        before the epoch.  In these cases, DST is disregarded.
        @param time  The time to calculate the time zone offset for
        @param input_utc  If true, the input time is in UTC, and will be
                          Converted to local time before considering DST
        @return  The time elapsed since 1/1/1970 as a duration
    */
    seconds GetLocalTZOffset(struct tm* time, bool input_utc);
}  // namespace fleece
