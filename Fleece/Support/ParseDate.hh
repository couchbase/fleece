//
//  ParseDate.h
//  Fleece
//
//  Copyright (c) 2017 Couchbase, Inc All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
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

