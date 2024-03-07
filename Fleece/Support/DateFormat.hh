#include "fleece/slice.hh"
#include <chrono>
#include <optional>
#include <variant>

namespace fleece {

    class DateFormat {
      public:
        static std::optional<DateFormat> parse(slice formatString);

        std::string format(int64_t timestamp);

      private:
        // %Y-%M-%DT%H:%M:%S
        static std::optional<DateFormat> parseTokenFormat(slice formatString);

        // 1111-11-11T11:11:11
        static std::optional<DateFormat> parseDateFormat(slice formatString);

        // Short = YY, Long = YYYY
        enum class Year : uint8_t { Short, Long };
        enum class Month : uint8_t {};
        enum class Day : uint8_t {};
        enum class Hours : uint8_t {};
        enum class Minutes : uint8_t {};
        enum class Seconds : uint8_t {};
        enum class Millis : uint8_t {};
        enum class Timezone : uint8_t {};
        enum class Separator : char { Space = ' ', T = 'T' };

        struct YMD {
            enum class Separator : char { Hyphen = '-', Slash = '/' };
            Year      year;
            Month     month;
            Day       day;
            Separator separator;
        };

        struct HMS {
            enum class Separator : char { Colon = ':' };
            Hours     hours;
            Minutes   minutes;
            Seconds   seconds;
            Millis    millis;
            Separator separator;
        };

        DateFormat(YMD ymd, Separator separator, std::optional<HMS> hms = {}, std::optional<Timezone> tz = {})
            : ymd{ymd}, separator{separator}, hms{hms}, tz{tz} {}

        YMD                     ymd;
        Separator               separator;
        std::optional<HMS>      hms;
        std::optional<Timezone> tz;
    };

}  // namespace fleece
