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

    static constexpr int64_t kInvalidDate = INT64_MIN;

    /** Parses a C string as an ISO-8601 date-time, returning a timestamp (milliseconds since
        1/1/1970), or kInvalidDate if the string is not valid. */
    int64_t ParseISO8601Date(const char* dateStr);

    /** Parses a C string as an ISO-8601 date-time, returning a timestamp (milliseconds since
        1/1/1970), or kInvalidDate if the string is not valid. */
    int64_t ParseISO8601Date(slice dateStr);

    /** Maximum length of a formatted ISO-8601 date. (Actually it's a bit bigger.) */
    static constexpr size_t kFormattedISO8601DateMaxSize = 40;

    /** Formats a timestamp (milliseconds since 1/1/1970) as an ISO-8601 date-time.
        @param buf  The location to write the formatted C string. At least
                    kFormattedISO8601DateMaxSize bytes must be available.
        @param timestamp  The timestamp (milliseconds since 1/1/1970).
        @param asUTC  True to format as UTC, false to use the local time-zone.
        @return  The formatted string (points to `buf`). */
    slice FormatISO8601Date(char buf[], int64_t timestamp, bool asUTC);

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
}

