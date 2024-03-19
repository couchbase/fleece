#include "DateFormat.hh"
#include "ParseDate.hh"
#include "date/date.h"
#include "fleece/slice.hh"
#include <optional>
#include <sstream>

namespace fleece {

    using namespace std::chrono;

    // The default format
    // YYYY-MM-DDThh:mm:ssTZD

    DateFormat::YMD DateFormat::YMD::kISO8601 = YMD{Year{}, Month{}, Day{}, YMD::Separator::Hyphen};

    DateFormat::HMS DateFormat::HMS::kISO8601 = HMS{Hours{}, Minutes{}, Seconds{}, Millis{}, HMS::Separator::Colon};

    DateFormat DateFormat::kISO8601 = DateFormat{YMD::kISO8601,
                                                 Separator::T,
                                                 HMS::kISO8601,
                                                 {Timezone::NoColon}};

    /** This parses a subset of the formatting tokens from "date.h", found 
     * here: https://howardhinnant.github.io/date/date.html#to_stream_formatting.
     * The valid tokens are:
     * %Y: Year (YYYY), %m: Month (MM), %d: Day (DD).
     * %F == %Y-%m-%d
     * %H: Hours (HH), %M: Minutes (MM), %S: Seconds (SS), %s: Milliseconds (sss) (Milliseconds is an addition to date.h tokens)
     * %T == %H:%M:%S.%s
     * %z: Timezone offset (Z for UTC, or offset in minutes (+/-)ZZZZ), %Ez: Timezone offset with colon ((+/-)ZZ:ZZ)
     * This parser is pretty restrictive, and only allows formats which match what we want.
     * YMD must always be full YMD, HMS must always be full HMS. Separators are restricted to 'T' and ' ' for YMD/HMS separator,
     * '-' and '/' for YMD separator, and ':' for HMS separator. Timezone offset is only allowed if HMS is present.
     * YMD can only be in ISO8601 order (no MDY allowed).
     *
     * ISO8601 can be represented as `%Y-%m-%dT%H:%M:%S%z` OR `%FT%T%z`
     * */
    std::optional<DateFormat> DateFormat::parseTokenFormat(slice formatString) {
        if ( formatString.size < 2 ) return {};

        // - YMD

        auto ymd = kISO8601.ymd;

        // %F == %Y-%m-%d
        if ( formatString[1] == 'F' ) {
            // ISO YMD, so we don't need to change `ymd`
            formatString.setStart(formatString.begin() + 2);
        } else {
            if ( formatString[1] == 'Y' ) {
                // Minimum %Y-%m-%d
                if ( formatString.size < 8 || formatString[3] != '%' || formatString[4] != 'm' || formatString[6] != '%'
                     || formatString[7] != 'd' ) {
                    return {};
                }
                switch ( formatString[2] ) {
                    case (char)YMD::Separator::Hyphen:
                        break;
                    case (char)YMD::Separator::Slash:
                        ymd.value().separator = YMD::Separator::Slash;
                        break;
                    default:
                        return {};
                }
                formatString.setStart(formatString.begin() + 8);
            } else {
                ymd = {};
            }
        }

        if ( formatString.empty() ) {
            if ( ymd.has_value() ) return DateFormat{ymd.value()};
            else
                return {};
        }

        // - SEPARATOR

        auto sep = kISO8601.separator;

        switch ( formatString[0] ) {
            case (char)Separator::Space:
                sep.value() = Separator::Space;
            case (char)Separator::T:
                formatString.setStart(formatString.begin() + 1);
                break;
            default:
                sep = {};
        }

        if ( formatString.size < 2 ) {
            if ( ymd.has_value() ) return DateFormat{ymd.value()};
            else
                return {};
        }

        if ( formatString[0] != '%' ) { return {}; }

        // - HMS

        auto hms = kISO8601.hms;

        // Equivalent to %H:%M:%S
        if ( formatString[1] == 'T' ) {
            // ISO HMS, so we don't need to change `hms`
            formatString.setStart(formatString.begin() + 2);
        } else {
            if ( formatString.size < 8 || formatString[1] != 'H' || formatString[3] != '%' || formatString[4] != 'M'
                 || formatString[6] != '%' || formatString[7] != 'S' ) {
                return {};
            }
            switch ( formatString[2] ) {
                case (char)HMS::Separator::Colon:
                    break;
                default:
                    return {};
            }
            formatString.setStart(formatString.begin() + 8);
            // Set Millis to None until we parse it later
            hms->millis = {};
        }

        if ( formatString.size < 2 ) {
            if ( ymd.has_value() ) {
                if ( hms.has_value() ) {
                    // If YMD + HMS, Separator is required.
                    if ( !sep.has_value() ) { return {}; }
                    return DateFormat{ymd.value(), sep.value(), hms.value()};
                }
                return DateFormat{ymd.value()};
            }
            if ( hms.has_value() ) { return DateFormat{hms.value()}; }
            return {};
        }

        // Millis
        // %s OR %.s
        if ( hms.has_value() ) {
            if ( formatString[0] == '%' && formatString[1] == 's' ) {
                hms.value().millis = {Millis{}};
                formatString.setStart(formatString.begin() + 2);
            } else if ( formatString.size > 2 && formatString[0] == '.' && formatString[1] == '%'
                        && formatString[2] == 's' ) {
                hms.value().millis = {Millis{}};
                formatString.setStart(formatString.begin() + 3);
            }
        }

        if ( formatString.size < 2 ) {
            if ( ymd.has_value() ) {
                if ( hms.has_value() ) {
                    // If YMD + HMS, Separator is required.
                    if ( !sep.has_value() ) { return {}; }
                    return DateFormat{ymd.value(), sep.value(), hms.value()};
                }
                return DateFormat{ymd.value()};
            }
            if ( hms.has_value() ) { return DateFormat{hms.value()}; }
            return {};
        }

        // - TIMEZONE

        auto tz = kISO8601.tz;

        if ( formatString[0] == '%' && formatString[1] == 'z' ) {
            tz.value() = Timezone::NoColon;
        } else if ( formatString.size >= 3 && formatString[0] == '%' && formatString[1] == 'E'
                    && formatString[2] == 'z' ) {
            tz.value() = Timezone::Colon;
        } else {
            // Format string contains additional invalid tokens
            return {};
        }

        if ( ymd.has_value() ) {
            if ( hms.has_value() ) {
                // If YMD + HMS, Separator is required.
                if ( !sep.has_value() ) { return {}; }
                if ( tz.has_value() ) { return DateFormat{ymd.value(), sep.value(), hms.value(), tz.value()}; }
                return DateFormat{ymd.value(), sep.value(), hms.value()};
            }
            return DateFormat{ymd.value()};
        }
        if ( hms.has_value() ) {
            if ( tz.has_value() ) return DateFormat{hms.value(), tz.value()};
            return DateFormat{hms.value()};
        }
        return {};
    }

    // 1111-11-11T11:11:11.111Z
    std::optional<DateFormat> DateFormat::parseDateFormat(slice formatString) {
        auto timezoneResult = parseTimezone(formatString);

        if ( timezoneResult.has_value() ) formatString = timezoneResult.value().second;

        auto tzResult =
                timezoneResult.has_value() ? std::optional(timezoneResult.value().first) : std::optional<Timezone>();

        auto hmsResult = parseHMS(formatString);

        if ( hmsResult.has_value() ) formatString = hmsResult.value().second;

        std::optional<char> separatorChar = formatString.empty() || !hmsResult.has_value()
                                                    ? std::optional<char>()
                                                    : formatString[formatString.size - 1];

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

    slice DateFormat::format(char buf[], const int64_t timestamp, minutes tzoffset, std::optional<DateFormat> fmt) {
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

            if ( f.hms.value().millis.has_value() && timestamp % 1000 ) {
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
        strncpy(buf, res.c_str(), res.length());

        return {buf, res.length()};
    }

    bool DateFormat::YMD::operator==(const YMD& other) const {
        return year == other.year && month == other.month && day == other.day && separator == other.separator;
    }

    bool DateFormat::HMS::operator==(const HMS& other) const {
        return hours == other.hours && minutes == other.minutes && seconds == other.seconds && millis == other.millis
               && separator == other.separator;
    }

    bool DateFormat::operator==(const DateFormat& other) const {
        return ymd == other.ymd && hms == other.hms && separator == other.separator && tz == other.tz;
    }

     DateFormat::operator std::string() const {
        std::stringstream stream {};
        if (ymd.has_value()) {
            const char sep = (char)ymd.value().separator;
            stream << "Y" << sep << "M" << sep << "D";
        }
        if (separator.has_value()) {
            stream << (char)separator.value();
        }
        if (hms.has_value()) {
            const char sep = (char)hms.value().separator;
            stream << "h" << sep << "m" << sep << "s";
            if (hms.value().millis.has_value()) {
                stream << ".SSS";
            }
        }
        if (tz.has_value()) {
            if (tz.value() == Timezone::Colon) {
                stream << "Ez";
            } else {
                stream << "z";
            }
        }
        return stream.str();
    }

    std::ostream& operator<<(std::ostream& os, DateFormat const& df) {
        os << std::string(df);
        return os;
    }

    std::ostream& operator<<(std::ostream& os, std::optional<DateFormat> const& odf) {
        if ( odf.has_value() ) {
            os << std::string(odf.value());
        } else {
            os << "None";
        }
        return os;
    }

}  // namespace fleece
