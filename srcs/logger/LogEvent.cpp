#include "logger/LogEvent.h"
#include "logger/Logger.h"
#include "logger/LogLevel.h"
#include <source_location>
#include <utility>


/* ======================== Event ======================== */
LogEvent::LogEvent(std::string logger_name,
             LogLevel loglevel,
             uint32_t elapse,
             std::thread::id thread_id,
             std::string thread_name,
             uint32_t co_id,
             std::time_t timestamp,
             std::source_location source_loc = std::source_location::current())
    : logger_name_{std::move(logger_name)}
    , loglevel_{ loglevel }
    , elapse_{ elapse }
    , thread_id_{ thread_id }
    , thread_name_{ std::move(thread_name) }
    , co_id_{ co_id }
    , timestamp_{ timestamp }
    , source_loc_{ source_loc }
{
}