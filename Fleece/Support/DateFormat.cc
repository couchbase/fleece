#include "DateFormat.hh"
#include "ParseDate.hh"
#include "date/date.h"
#include "fleece/slice.hh"
#include <optional>
#include <sstream>

namespace fleece {

    using namespace std::chrono;

    // YYYY-MM-DDThh:mm:ssTZD
    DateFormat DateFormat::kISO8601 = DateFormat{YMD{{}, {}, {}, YMD::Separator::Hyphen},
                                                 Separator::T,
                                                 HMS{{}, {}, {}, HMS::Separator::Colon},
                                                 {Timezone{}}};

    // %Y-%M-%DT%H:%M:%S
    // TODO: Implement!
    std::optional<DateFormat> DateFormat::parseTokenFormat(slice formatString) { return {}; }

    // 1111-11-11T11:11:11.111Z
    std::optional<DateFormat> DateFormat::parseDateFormat(slice formatString) {
        auto timezoneResult = parseTimezone(formatString);

        if ( timezoneResult.has_value() ) formatString = timezoneResult.value().second;

        auto tzResult =
                timezoneResult.has_value() ? std::optional(timezoneResult.value().first) : std::optional<Timezone>();

        auto hmsResult = parseHMS(formatString);

        if ( hmsResult.has_value() ) formatString = hmsResult.value().second;

        std::optional<char> separatorChar =
                formatString.empty() || !hmsResult.has_value() ? std::optional<char>() : formatString[formatString.size - 1];

        std::optional<Separator> separator{};

        if ( separatorChar.has_value() ) {
            char sep = separatorChar.value();
            if ( sep == (char)Separator::Space ) {
                separator = Separator::Space;
            } else if ( sep == (char)Separator::T ) {
                separator = Separator::T;
            } else {
                // Invalid YMD/HMS Separator
                return {};
            }
            formatString = formatString.upTo(formatString.size - 1);
        }

        auto ymdResult = parseYMD(formatString);

        if ( separator.has_value() ) {
            // We must have YMD and HMS if there is a separator.
            if ( !ymdResult.has_value() || !hmsResult.has_value() ) { return {}; }
        }

        // We must have HMS if we have timezone specifier.
        if ( timezoneResult.has_value() && !hmsResult.has_value() ) { return {}; }

        if ( ymdResult.has_value() ) {
            if ( hmsResult.has_value() ) {
                if ( !separator.has_value() ) return {};
                return {{ymdResult.value(), separator.value(), hmsResult.value().first, tzResult}};
            }
            return {DateFormat{ymdResult.value()}};
        } else if ( hmsResult.has_value() ) {
            return {DateFormat{hmsResult.value().first, tzResult}};
        } else {
            // We must have _either_ YMD or HMS.
            return {};
        }
    }

    std::optional<std::pair<DateFormat::Timezone, slice>> DateFormat::parseTimezone(const slice formatString) {
        // Default to No Colon
        if ( *(formatString.end() - 1) == 'Z' ) return {{Timezone::NoColon, formatString.upTo(formatString.size - 1)}};
        // Minimum 5 `+0000`
        if ( formatString.size < 5 ) return {};
        const bool colon = *(formatString.end() - 3) == ':';

        const size_t start = colon ? formatString.size - 6 : formatString.size - 5;

        if ( formatString[start] == '+' || formatString[start] == '-' ) {
            if ( colon ) {
                return {{Timezone::Colon, formatString.upTo(start)}};
            } else {
                return {{Timezone::NoColon, formatString.upTo(start)}};
            }
        }

        return {};
    }

    // Input some string which may or may not contain HMS but does NOT contain timezone. That should have already been
    // stripped by `parseTimezone` (ie "1111-11-11T11:11:11.111" or "11:11").
    // Returns the parsed HMS and the format string with HMS removed, or None if valid HMS was not found.
    std::optional<std::pair<DateFormat::HMS, slice>> DateFormat::parseHMS(slice formatString) {
        // Minimum 11:11:11
        if ( formatString.size < 8 ) return {};
        const bool millis = *(formatString.end() - 4) == '.';

        // If we have millis, we must have minimum 11:11:11.111 (12 chars)
        if ( millis && formatString.size < 12 ) { return {}; }

        // Shorten to get rid of millis, input minimum is now 11:11:11
        if ( millis ) { formatString = formatString.upTo(formatString.size - 4); }

        // Check HMS is formatted correctly
        if ( !(*(formatString.end() - 3) == ':' && *(formatString.end() - 6) == ':') ) { return {}; }

        std::optional<Millis> ms = {};

        if ( millis ) ms = {Millis{}};

        const size_t start = formatString.size - 8;

        if ( ms.has_value() ) {
            return {{{Hours{}, Minutes{}, Seconds{}, ms.value(), HMS::Separator::Colon}, formatString.upTo(start)}};
        }

        return {{{Hours{}, Minutes{}, Seconds{}, HMS::Separator::Colon}, formatString.upTo(start)}};
    }

    // Input some string which may or may not contain YMD but does NOT contain HMS, Timezone, or the YMD/HMS separator (i.e. 'T').
    // This should be called after already calling `parseTimezone` ,`parseHMS`, and removing the separator.
    std::optional<DateFormat::YMD> DateFormat::parseYMD(slice formatString) {
        // Minimum 1111-11-11
        if ( formatString.size < 10 ) return {};

        auto separator = YMD::Separator::Hyphen;

        if ( *(formatString.end() - 3) == '-' && *(formatString.end() - 6) == '-' ) {
        } else if ( *(formatString.end() - 3) == '/' && *(formatString.end() - 6) == '/' ) {
            separator = YMD::Separator::Slash;
        } else {
            return {};
        }

        return {YMD{Year{}, Month{}, Day{}, separator}};
    }

    std::optional<DateFormat> DateFormat::parse(const slice formatString) {
        if ( formatString.empty() ) { return {}; }
        if ( formatString[0] == '%' ) { return parseTokenFormat(formatString); }
        return parseDateFormat(formatString);
    }

    slice DateFormat::format(char buf[], int64_t timestamp, bool asUTC, std::optional<DateFormat> fmt) {
        if ( asUTC ) {
            return format(buf, timestamp, minutes{0}, fmt);
        } else {
            const milliseconds millis{timestamp};
            auto               temp           = FromTimestamp(floor<seconds>(millis));
            const seconds      offset_seconds = GetLocalTZOffset(&temp, false);
            return format(buf, timestamp, duration_cast<minutes>(offset_seconds), fmt);
        }
    }

    slice DateFormat::format(char buf[], int64_t timestamp, minutes tzoffset, std::optional<DateFormat> fmt) {
        if ( timestamp == kInvalidDate ) {
            *buf = 0;
            return nullslice;
        }

        std::ostringstream stream{};

        const milliseconds millis{milliseconds{timestamp} + duration_cast<milliseconds>(tzoffset)};
        const auto         tm = date::local_time<milliseconds>{millis};

        const seconds offset_seconds{tzoffset};

        const DateFormat f = fmt.has_value() ? fmt.value() : kISO8601;

        if ( f.ymd.has_value() ) { stream << date::format("%F", tm); }

        if ( f.hms.has_value() ) {
            if ( f.ymd.has_value() ) { stream << (char)f.separator.value(); }

            if ( f.hms.value().millis.has_value() ) {
                stream << date::format("%T", tm);
            } else {
                const auto secs = duration_cast<seconds>(millis);
                stream << date::format("%T", date::local_seconds(secs));
            }

            if ( f.tz.has_value() ) {
                if ( offset_seconds.count() == 0 ) {
                    stream << 'Z';
                } else {
                    if ( f.tz.value() == Timezone::Colon ) to_stream(stream, "%Ez", tm, nullptr, &offset_seconds);
                    else
                        to_stream(stream, "%z", tm, nullptr, &offset_seconds);
                }
            }
        }

        const std::string res = stream.str();
        std::strncpy(buf, res.c_str(), res.length());

        return {buf, res.length()};
    }
}  // namespace fleece
