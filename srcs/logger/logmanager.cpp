#include "logger/logger.h"
#include "logger/logappender.hpp"
#include "logger/loggermanager.h"


namespace LogT{

LoggerManager::LoggerManager()
    : root_(new Logger("root"))
    , loggers_{{"root", root_}}

{
    root_->AddAppender(std::make_shared<Appender<StdoutAppender>>());
    Init();
}

/**
 * todo: load logger config from config file
 */
void LoggerManager::Init()
{

}

auto LoggerManager::GetLogger(std::string_view logger_name)
    -> Sptr<Logger>
{
    auto lk_guard = std::lock_guard<MutexType>{mtx_};
    if (auto it = loggers_.find(std::string(logger_name)); it != loggers_.end())
        return it->second;
    auto logger = Sptr<Logger>(new Logger(std::string(logger_name)));
    loggers_[std::string(logger_name)] = logger;
    return logger;
}

} //namespace LogT