#include "DateFormat.hh"
#include <cctype>
#include <exception>
#include <optional>

namespace fleece {
    static size_t separatedSection(slice input, char separator) {
        auto foundSeparator = input.findByte((unsigned char)separator);
        if ( !foundSeparator ) return 0;
        return input.offsetOf(foundSeparator);
    }

    // %Y-%M-%DT%H:%M:%S
    std::optional<DateFormat> DateFormat::parseTokenFormat(slice formatString) {}

    // 1111-11-11T11:11:11.111Z
    std::optional<DateFormat> DateFormat::parseDateFormat(slice formatString) {
        auto hyphenYearSize = separatedSection(formatString, (char)DateFormat::YMD::Separator::Hyphen);
        auto slashYearSize  = separatedSection(formatString, (char)DateFormat::YMD::Separator::Slash);

        auto yearSeparator = DateFormat::YMD::Separator::Hyphen;
        auto year          = DateFormat::Year::Long;

        if ( hyphenYearSize == 4 || hyphenYearSize == 2 ) {
            year = hyphenYearSize == 4 ? DateFormat::Year::Long : DateFormat::Year::Short;
        } else if ( slashYearSize == 4 || slashYearSize == 2 ) {
            yearSeparator = DateFormat::YMD::Separator::Slash;
            year          = slashYearSize == 4 ? DateFormat::Year::Long : DateFormat::Year::Short;
        } else {
            return {};
        }

        // Skip past the year (now 11-11T11:11:11.111Z)
        formatString = formatString.from(formatString.findByte((unsigned char)yearSeparator) + 1);

        if ( separatedSection(formatString, (unsigned char)yearSeparator) != 2 ) { return {}; }

        // Skip past month (now 11T11:11:11.111Z)
        formatString = formatString.from(formatString.findByte((unsigned char)yearSeparator) + 1);

        auto tDaySize     = separatedSection(formatString, (char)DateFormat::Separator::T);
        auto spaceDaySize = separatedSection(formatString, (char)DateFormat::Separator::Space);

        auto ymdHmsSeparator = DateFormat::Separator::T;

        if ( tDaySize == 2 ) {
        } else if ( spaceDaySize == 2 ) {
            ymdHmsSeparator = DateFormat::Separator::Space;
        } else if ( formatString.size == 2 ) {
            return {DateFormat{DateFormat::YMD{year, {}, {}, yearSeparator}, ymdHmsSeparator, {}, {}}};
        } else {
            // Make sure the bit after this is Z or Time
        }

        // Skip past day (now 11:11:11.111Z)
        formatString = formatString.from(formatString.findByte((unsigned char)ymdHmsSeparator) + 1);

        auto timeSeparator = DateFormat::HMS::Separator::Colon;

        if ( separatedSection(formatString, (char)timeSeparator) != ) }

    std::optional<DateFormat> DateFormat::parse(slice formatString) {
        if ( formatString.empty() ) { return {}; }
        unsigned char first = formatString[0];
        if ( first == '%' ) {
            return parseTokenFormat(formatString);
        } else {
            return parseDateFormat(formatString);
        }
    }

    std::string DateFormat::format(int64_t timestamp) {}
}  // namespace fleece
