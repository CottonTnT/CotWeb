#include "logger/logutil.h"
#include "common/util.h"
#include "logger/loglevel.h"

namespace LogT {

auto LevelToString(Level level)
    -> std::string_view
{

    switch (level)
    {

#define IFCODE(levelvalue, levelname) \
    case (levelvalue):{\
        return #levelname;\
    }
        IFCODE(Level::ERROR, ERROR);
        IFCODE(Level::WARN, WARN);
        IFCODE(Level::INFO, INFO);
        IFCODE(Level::DEBUG, DEBUG);
        IFCODE(Level::ALL, ALL);
#undef IFCODE
    }
}

/**
 * @todo: should check the hash collision
 */
auto StringToLevel(std::string_view str)
    -> Level
{
    switch (UtilT::cHashString(str))
    {
#define IFCODE(levelname, levelvalue) \
    case (UtilT::cHashString(#levelname)):{\
        return levelvalue;\
    }\

    IFCODE(ERROR, Level::ERROR);
    IFCODE(error, Level::ERROR);
    IFCODE(WARN, Level::WARN);
    IFCODE(warn, Level::WARN);
    IFCODE(INFO, Level::INFO);
    IFCODE(info, Level::INFO);
    IFCODE(DEBUG, Level::DEBUG);
    IFCODE(debug, Level::DEBUG);
#undef IFCODE
    default:
           return Level::ALL;
    }
}


} // namespace LogT


