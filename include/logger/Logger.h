#ifndef __LOG_H__
#define __LOG_H__

#include <list>
#include <source_location>
#include <thread>
#include <utility>

#include "common/alias.h"
#include "common/curthread.h"
#include "logger/AppenderFacade.h"
#include "LogEvent.h"
#include "LogLevel.h"

class Logger;
class LoggerManager;
class LogEvent;
class AppenderFacade;

/**
 * @brief 日志器，用于输出日志。这个类是直接与用户进行交互的类，提供 log 方法用于输出日志事件。不带root logger。
 *  Logger的实现包含了日志级别，日志器名称，创建时间，以及一个 LogAppender 数组，
 * 日志事件由 log 方法输出，log 方法首先判断日志级别是否达到本 Logger 的级别要求，
 * 是则将日志传给各个 LogAppender 进行输出，否则抛弃这条日志。
 */
class Logger : public std::enable_shared_from_this<Logger> {
private:
    inline static std::atomic<uint32_t> auto_logger_id_ = 0;
    std::string name_;                          // 日志器名称
    LogLevel level_ = LogLevel::ALL;          // 日志级别
    std::list<Sptr<AppenderFacade>> appenders_; // Appender集合

public:
    explicit Logger(std::string name = std::to_string(++auto_logger_id_));

    void log(const LogEvent& event) const;
    void log(const LogEvent& event, std::error_code& ec) const;

    void addAppender(Sptr<AppenderFacade> appender);
    void delAppender(Sptr<AppenderFacade> appender);
    void clearAppender();

    auto getLoggerName() const& -> const std::string& { return name_; }

    auto getLoggerName() && -> std::string { return std::move(name_); }
    auto getLogLevel() const
        -> LogLevel { return level_; }

    auto setLogLevel(LogLevel level)
        -> void { level_ = level; }
};

template <typename... Args>
auto LogFMTLevel(const Logger& logger, LogLevel loglevel, std::format_string<Args...> fmt, Args&&... args)
    -> void
{
    auto event = LogEvent {logger.getLoggerName(), loglevel, 0, std::this_thread::get_id(), CurThr::GetName(), 0, 0, std::source_location::current()};
    event.print(fmt, std::forward<Args>(args)...);
    logger.log(event);
}

inline auto LogFMTLevel(const Logger& logger, LogLevel loglevel)
    -> void
{
    logger.log(LogEvent {logger.getLoggerName(), loglevel, 0, std::this_thread::get_id(), CurThr::GetName(), 0, 0, std::source_location::current()});
}

template <LogLevel loglevel = LogLevel::DEBUG, typename... Args>
void LOG(Sptr<Logger> logger, std::format_string<Args...> fmt, Args&&... args)
{
    LogFMTLevel(*logger, loglevel, fmt, std::forward<Args>(args)...);
}

template <LogLevel loglevel = LogLevel::DEBUG>
void LOG(Sptr<Logger> logger)
{
    LogFMTLevel(*logger, loglevel);
}
#endif