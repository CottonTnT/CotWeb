
#include "logger/Logger.h"
#include "logger/LogEvent.h"
#include "logger/LogLevel.h"
#include "logger/LogErrorCode.h"

#include <algorithm>
#include <cstdlib>
#include <unordered_map>

/* ======================== Logger ======================== */
Logger::Logger(std::string name)
    : name_(std::move(name))
{
}

void Logger::addAppender(Sptr<AppenderFacade> appender)
{
    appenders_.push_back(appender);
}

void Logger::delAppender(Sptr<AppenderFacade> appender)
{
    for (auto it = std::begin(appenders_); it != std::end(appenders_); it++)
    {
        if (*it == appender)
        {
            appenders_.erase(it);
            break;
        }
    }
}

void Logger::clearAppender()
{
    appenders_.clear();
}

void Logger::log(const LogEvent& event, std::error_code& ec) const
{
    ec = {};
    if (event.getLevel() < level_)
    {
        return; // 不算错误，直接忽略
    }
    if (appenders_.empty())
    {
        ec = LogErrcCode::NoAppender;
        return;
    }
    if (event.getLevel() >= level_)
    {
        std::ranges::for_each(appenders_,
                              [&event](Sptr<AppenderFacade> appender) {
                                  appender->log(event);
                              });
    }
    if (event.getLevel() == LogLevel::SYSFATAL)
    {
        // todo: bug here, flush all appenders before exit
        ::exit(EXIT_FAILURE);
    }
}

void Logger::log(const LogEvent& event) const
{

    std::error_code ec;
    log(event, ec);
    if (ec)
    {
        throw std::system_error(ec);
    }
}
