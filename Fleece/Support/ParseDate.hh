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

namespace fleece {

    static constexpr int64_t kInvalidDate = INT64_MIN;

    /*
     ** Components of a date that can be extracted, diffed, or added using date-time functions
     */
    typedef enum {
        kDateComponentMillennium,  /* 1000 years */
        kDateComponentCentury,     /* 100 years */  
        kDateComponentDecade,      /* 10 years */  
        kDateComponentYear,        /* 146097 / 400 days */  
        kDateComponentQuarter,     /* 3 months */
        kDateComponentMonth,       /* 1/12 years */  
        kDateComponentWeek,        /* 7 days */
        kDateComponentDay,         /* 24 hours */
        kDateComponentHour,        /* 60 minutes */
        kDateComponentMinute,      /* 60 seconds */
        kDateComponentSecond,      /* 1000 milliseconds */  
        kDateComponentMillisecond, /* base unit */
        kDateComponentInvalid
    } DateComponent;

    /*
     ** A structure for holding a single date and time.
     */
    typedef struct DateTime DateTime;
    struct DateTime {
        int64_t iJD;       /* The julian day number times 86400000 */
        int Y, M, D;       /* Year, month, and day */
        int h, m;          /* Hour and minutes */
        int tz;            /* Timezone offset in minutes */
        double s;          /* Seconds */
        char validYMD;     /* True (1) if Y,M,D are valid */
        char validHMS;     /* True (1) if h,m,s are valid */
        char validJD;      /* True (1) if iJD is valid */
        char validTZ;      /* True (1) if tz is valid */
        char separator;    /* The character used to separate the date and time (T or space) */
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
    DateTime FromMillis(int64_t millis);

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
    slice FormatISO8601Date(char buf[], int64_t timestamp, bool asUTC, const DateTime* format);

    /** Formats a timestamp (milliseconds since 1/1/1970) as an ISO-8601 date-time.
        @param buf  The location to write the formatted C string. At least
                    kFormattedISO8601DateMaxSize bytes must be available.
        @param timestamp  The timestamp (milliseconds since 1/1/1970).
        @param tz       The timezone offset from UTC in minutes
        @param format The model to use for formatting (i.e. which portions to include).
                      If null, then the full ISO-8601 format is used
        @return  The formatted string (points to `buf`). */
    slice FormatISO8601Date(char buf[], int64_t timestamp, int tz, const DateTime* format);
}

