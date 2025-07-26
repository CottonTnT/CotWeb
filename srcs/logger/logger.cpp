#include "logger/logger.h"
#include "logger/logappender.hpp"
#include "logger/logevent.h"
#include "logger/loglevel.h"
#include "util.h"

#include <unordered_map>

namespace LogT {

/* ======================== Logger ======================== */
Logger::Logger(std::string name)
    : name_(std::move(name))
    , level_(Level::DEBUG)
    , create_time_(UtilT::GetElapseMs())
{

}

void Logger::AddAppender(Sptr<AppenderProxyBase> appender)
{
    auto lk_guard = std::lock_guard<MutexType>(mtx_);
    appenders_.push_back(appender);
}

void Logger::DelAppender(Sptr<AppenderProxyBase> appender)
{
    auto lk_guard = std::lock_guard<MutexType>(mtx_);
    for (auto it = std::begin(appenders_); it != std::end(appenders_); it++)
    {
        if (*it == appender)
        {
            appenders_.erase(it);
            break;
        }
    }
}

void Logger::ClearAppender()
{
    auto lk_guard = std::lock_guard<MutexType>(mtx_);
    appenders_.clear();
}

void Logger::Log(Sptr<Event> event)
{
    if (event->GetLevel() < level_)
        return;
    for (auto& i : appenders_)
    {
        i->Log(event);
    }
    
}

} //namespace LogT
