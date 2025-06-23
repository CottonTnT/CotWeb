#include "logger/logutil.h"
#include "logger/loglevel.h"
#include "util.h"

namespace LogT {

auto LevelToString(Level level)
    -> std::optional<std::string_view>
{

    switch (level)
    {

#define IFCODE(levelvalue, levelname) \
    case (levelvalue):{\
        return #levelname;\
    }

        IFCODE(Level::FATAL, FATAL);
        IFCODE(Level::ALERT, ALERT);
        IFCODE(Level::CRIT, CRIT);
        IFCODE(Level::ERROR, ERROR);
        IFCODE(Level::WARN, WARN);
        IFCODE(Level::NOTICE, NOTICE);
        IFCODE(Level::INFO, INFO);
        IFCODE(Level::DEBUG, DEBUG);
        IFCODE(Level::NOTSET, NOTSET);
#undef IFCODE
        default:
            return std::nullopt;
    }
}

/**
 * @todo: should check the hash collision
 */
auto StringToLevel(std::string_view str)
    -> std::optional<Level>
{
    switch (UtilT::cHashString(str))
    {
#define IFCODE(levelname, levelvalue) \
    case (UtilT::cHashString(#levelname)):{\
        return levelvalue;\
    }\

    IFCODE(FATAL, Level::FATAL);
    IFCODE(fatal, Level::FATAL);
    IFCODE(ALERT, Level::ALERT);
    IFCODE(alert, Level::ALERT);
    IFCODE(CRIT, Level::CRIT);
    IFCODE(crit, Level::CRIT);
    IFCODE(ERROR, Level::ERROR);
    IFCODE(error, Level::ERROR);
    IFCODE(WARN, Level::WARN);
    IFCODE(warn, Level::WARN);
    IFCODE(NOTICE, Level::NOTICE);
    IFCODE(notice, Level::NOTICE);
    IFCODE(INFO, Level::INFO);
    IFCODE(info, Level::INFO);
    IFCODE(DEBUG, Level::DEBUG);
    IFCODE(debug, Level::DEBUG);
#undef IFCODE
    default:
           return std::nullopt;
    }
}


} // namespace LogT


