#include "logger/LogLevel.h"
#include "common/util.hpp"

namespace X {
auto logLevel2String(LogLevel level)
    -> std::string_view
{

    switch (level) {

#define IFCODE(levelvalue, levelname) \
    case (levelvalue): {              \
        return #levelname;            \
    }

        IFCODE(LogLevel::ERROR, ERROR);
        IFCODE(LogLevel::WARN, WARN);
        IFCODE(LogLevel::INFO, INFO);
        IFCODE(LogLevel::DEBUG, DEBUG);
#undef IFCODE
    }
}

/**
 * @todo: should check the hash collision
 */
auto string2LogLevel(std::string_view str)
    -> LogLevel
{
    switch (UtilT::hashString(str)) {
#define IFCODE(levelname, levelvalue)       \
    case (UtilT::hashString(#levelname)): { \
        return levelvalue;                  \
    }

        // 您提供的部分 (ERROR, WARN, INFO, DEBUG)
        IFCODE(ERROR, LogLevel::ERROR);
        IFCODE(error, LogLevel::ERROR);
        IFCODE(WARN, LogLevel::WARN);
        IFCODE(warn, LogLevel::WARN);

        IFCODE(INFO, LogLevel::INFO);
        IFCODE(info, LogLevel::INFO);
        IFCODE(DEBUG, LogLevel::DEBUG);
        IFCODE(debug, LogLevel::DEBUG);

#undef IFCODE
    }
}

auto operator<=(LogLevel lhs, LogLevel rhs)
    -> bool
{
    return static_cast<std::underlying_type_t<LogLevel>>(lhs) <= static_cast<std::underlying_type_t<LogLevel>>(rhs);
}

auto operator>(LogLevel lhs, LogLevel rhs)
    -> bool
{
    return not(lhs <= rhs);
}

auto operator==(LogLevel one, LogLevel two)
    -> bool
{
    return static_cast<std::underlying_type_t<LogLevel>>(one) == static_cast<std::underlying_type_t<LogLevel>>(two);
}

auto operator>=(LogLevel lhs, LogLevel rhs)
    -> bool
{
    return lhs > rhs or lhs == rhs;
}

auto operator<(LogLevel lhs, LogLevel rhs)
    -> bool
{
    return not(lhs >= rhs);
}
} // namespace X