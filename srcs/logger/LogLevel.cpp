#include "logger/LogLevel.h"
#include "common/util.hpp"

auto LevelToString(LogLevel level)
    -> std::string_view
{

    switch (level)
    {

#define IFCODE(levelvalue, levelname) \
    case (levelvalue): {              \
        return #levelname;            \
    }

        IFCODE(LogLevel::SYSFATAL, SYSFATAL);
        IFCODE(LogLevel::SYSERR, SYSERR);
        IFCODE(LogLevel::FATAL, FATAL);
        IFCODE(LogLevel::ERROR, ERROR);
        IFCODE(LogLevel::WARN, WARN);
        IFCODE(LogLevel::TRACE, TRACE); // 缺失的 TRACE
        IFCODE(LogLevel::INFO, INFO);
        IFCODE(LogLevel::DEBUG, DEBUG);
        IFCODE(LogLevel::ALL, ALL);

#undef IFCODE
    }
}

/**
 * @todo: should check the hash collision
 */
auto StringToLevel(std::string_view str)
    -> LogLevel
{
    switch (UtilT::cHashString(str))
    {
#define IFCODE(levelname, levelvalue)        \
    case (UtilT::cHashString(#levelname)): { \
        return levelvalue;                   \
    }
        // SYSFATAL
        IFCODE(SYSFATAL, LogLevel::SYSFATAL);
        IFCODE(sysfatal, LogLevel::SYSFATAL);

        // SYSERR
        IFCODE(SYSERR, LogLevel::SYSERR);
        IFCODE(syserr, LogLevel::SYSERR);

        // FATAL
        IFCODE(FATAL, LogLevel::FATAL);
        IFCODE(fatal, LogLevel::FATAL);

        // 您提供的部分 (ERROR, WARN, INFO, DEBUG)
        IFCODE(ERROR, LogLevel::ERROR);
        IFCODE(error, LogLevel::ERROR);
        IFCODE(WARN, LogLevel::WARN);
        IFCODE(warn, LogLevel::WARN);

        // TRACE
        IFCODE(TRACE, LogLevel::TRACE); // 缺失的 TRACE
        IFCODE(trace, LogLevel::TRACE); // 缺失的 trace

        IFCODE(INFO, LogLevel::INFO);
        IFCODE(info, LogLevel::INFO);
        IFCODE(DEBUG, LogLevel::DEBUG);
        IFCODE(debug, LogLevel::DEBUG);

        // ALL (通常也需要支持)
        IFCODE(ALL, LogLevel::ALL);
        IFCODE(all, LogLevel::ALL);
#undef IFCODE
        default:
            return LogLevel::ALL;
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