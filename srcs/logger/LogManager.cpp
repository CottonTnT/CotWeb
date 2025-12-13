#include <memory>
#include <mutex>

#include "logger/Logger.h"
#include "logger/LogAppender.h"
#include "logger/LoggerManager.h"




LoggerManager::LoggerManager()
    : root_(new Logger("root"))
    , loggers_{
    {
        "root", root_
        }}
{
    root_->addAppender(std::make_shared<AppenderProxy<StdoutAppender>>());
    init_();
}

/**
 * todo: load logger config from config file
 */
void LoggerManager::init_()
{
}

auto LoggerManager::getLogger(std::string_view logger_name)
    -> Sptr<Logger>
{
    auto _ = std::lock_guard<std::mutex>{mtx_};

    if (auto it = loggers_.find(logger_name ); it != loggers_.end())
        return it->second;
    
    auto logger = std::make_shared<Logger>( std::string{logger_name} );

    loggers_.emplace(logger_name, logger);

    return logger;
}
