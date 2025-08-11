#include "logger/logger.h"
#include "common/util.h"
#include "logger/logappender.h"
#include "logger/logevent.h"
#include "logger/loglevel.h"
#include "logger/logutil.h"

#include <algorithm>
#include <unordered_map>

namespace LogT {

/* ======================== Logger ======================== */
Logger::Logger(std::string name)
    : name_(std::move(name))
    , create_time_(UtilT::GetElapseMs())
{

}

void Logger::AddAppender(Sptr<AppenderProxyBase> appender)
{
    auto _ = Cot::LockGuard<MutexType>(mtx_);
    appenders_.push_back(appender);
}

void Logger::DelAppender(Sptr<AppenderProxyBase> appender)
{
    auto _ = Cot::LockGuard<MutexType>(mtx_);
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
    auto _ = Cot::LockGuard<MutexType>(mtx_);
    appenders_.clear();
}

void Logger::Log(Sptr<Event> event)
{
    if (event->GetLevel() >= level_)
    {
        std::ranges::for_each(appenders_,
                          [event](Sptr<AppenderProxyBase> appender)
                            -> void{
                                appender->Log(event);
                          });
    }
}
} //namespace LogT
