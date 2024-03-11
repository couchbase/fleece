#pragma once

#include "fleece/slice.hh"
#include <chrono>
#include <optional>

namespace fleece {

    static constexpr int64_t kInvalidDate = INT64_MIN;

    /** Maximum length of a formatted ISO-8601 date. (Actually it's a bit bigger.) */
    static constexpr size_t kFormattedISO8601DateMaxSize = 40;

    class DateFormat {
      public:
        static std::optional<DateFormat> parse(slice formatString);

        /** Formats a timestamp (milliseconds since 1/1/1970) as an ISO-8601 date-time.
        @param buf  The location to write the formatted C string. At least
                    kFormattedISO8601DateMaxSize bytes must be available.
        @param timestamp  The timestamp (milliseconds since 1/1/1970).
        @param asUTC  True to format as UTC, false to use the local time-zone.
        @param fmt The model to use for formatting (i.e. which portions to include).
                      If null, then the full ISO-8601 format is used
        @return  The formatted string (points to `buf`). */
        static slice format(char buf[], int64_t timestamp, bool asUTC, std::optional<DateFormat> fmt);

        /** Formats a timestamp (milliseconds since 1/1/1970) as an ISO-8601 date-time.
        @param buf  The location to write the formatted C string. At least
                    kFormattedISO8601DateMaxSize bytes must be available.
        @param timestamp  The timestamp (milliseconds since 1/1/1970).
        @param tzoffset   The timezone offset from UTC in minutes
        @param fmt The model to use for formatting (i.e. which portions to include).
                      If null, then the full ISO-8601 format is used
        @return  The formatted string (points to `buf`). */
        static slice format(char buf[], int64_t timestamp, std::chrono::minutes tzoffset,
                            std::optional<DateFormat> fmt);

        static DateFormat kISO8601;

      private:
        enum class Year : uint8_t {};
        enum class Month : uint8_t {};
        enum class Day : uint8_t {};
        enum class Hours : uint8_t {};
        enum class Minutes : uint8_t {};
        enum class Seconds : uint8_t {};
        enum class Millis : uint8_t {};
        enum class Timezone : uint8_t { NoColon, Colon };
        enum class Separator : char { Space = ' ', T = 'T' };

        struct YMD {
            enum class Separator : char { Hyphen = '-', Slash = '/' };

            YMD(const Year y, const Month m, const Day d, const Separator sep)
                : year(y), month(m), day(d), separator(sep) {}

            Year      year;
            Month     month;
            Day       day;
            Separator separator;
        };

        struct HMS {
            enum class Separator : char { Colon = ':' };

            // 11:11:11
            HMS(const Hours h, const Minutes m, const Seconds s, const Separator sep)
                : hours{h}, minutes{m}, seconds{s}, separator{sep} {}

            // 11:11:11.111
            HMS(const Hours h, const Minutes m, const Seconds s, const Millis ms, const Separator sep)
                : hours{h}, minutes{m}, seconds{s}, millis{ms}, separator{sep} {}

            Hours                  hours;
            Minutes                minutes;
            std::optional<Seconds> seconds;
            std::optional<Millis>  millis;
            Separator              separator;
        };

        // 1111-11-11T11:11:11(Z)
        DateFormat(YMD ymd, Separator separator, HMS hms, const std::optional<Timezone> tz = {})
            : ymd{ymd}, separator{separator}, hms{hms}, tz{tz} {}

        // 11-11-11
        explicit DateFormat(YMD ymd) : ymd{ymd} {}

        // 11:11:11(Z)
        explicit DateFormat(HMS hms, const std::optional<Timezone> tz = {}) : hms{hms}, tz{tz} {}

        // %Y-%M-%DT%H:%M:%S
        static std::optional<DateFormat> parseTokenFormat(slice formatString);

        // 1111-11-11T11:11:11
        static std::optional<DateFormat> parseDateFormat(slice formatString);

        static std::optional<std::pair<Timezone, slice>> parseTimezone(slice formatString);

        static std::optional<std::pair<HMS, slice>> parseHMS(slice formatString);

        static std::optional<YMD> parseYMD(slice formatString);

        std::optional<YMD>       ymd;
        std::optional<Separator> separator;
        std::optional<HMS>       hms;
        std::optional<Timezone>  tz;
    };

}  // namespace fleece
