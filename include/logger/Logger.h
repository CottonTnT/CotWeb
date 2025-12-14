#ifndef __LOG_H__
#define __LOG_H__

#include <chrono>
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
    LogLevel level_ = LogLevel::ALL;            // 日志级别
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
    auto isLevelEnabled(LogLevel level) const
        -> bool
    {
        return level >= level_;
    }

    auto setLogLevel(LogLevel level)
        -> void { level_ = level; }
};

template <typename... Args>
inline auto LogFMT(const Logger& logger, LogLevel loglevel, std::source_location source_info, std::format_string<Args...> fmt, Args&&... args)
    -> void
{
    auto event = LogEvent {
        logger.getLoggerName(),
        loglevel,
        0,
        std::this_thread::get_id(),
        CurThr::GetName(),
        0,
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()),
        source_info};
        event.print(fmt, std::forward<Args>(args)...);

    logger.log(event);
}

inline auto Log(const Logger& logger, LogLevel loglevel, std::source_location source_info)
    -> void
{
    logger.log(LogEvent {
        logger.getLoggerName(),
        loglevel,
        0,
        std::this_thread::get_id(),
        CurThr::GetName(),
        0,
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()),
        source_info});
}

// 宏定义：现在宏负责捕获位置信息并传递给模板函数
#define LOG_FMT_HELPER(logger_ptr, loglevel, fmt, ...) \
    do {                                               \
        LogFMT(*(logger_ptr),                     \
                    loglevel,                          \
                    std::source_location::current(),   \
                    fmt,                               \
                    ##__VA_ARGS__);                      \
    } while (0)


#define LOG_HELPER(logger_ptr, loglevel)             \
    do {                                             \
        Log(*(logger_ptr),                   \
                    loglevel,                        \
                    std::source_location::current()) \
    } while (0)

// 假设 logger_ptr 指向的 Logger 对象有一个 isLevelEnabled(LogLevel) 方法
// 用于快速判断是否需要记录日志。

// --- 带有格式化参数的宏 ---

#define LOG_SYSFATAL_FMT(logger_ptr, fmt, ...) \
    do { \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(LogLevel::SYSFATAL)) { \
            break; \
        } \
        LOG_FMT_HELPER(logger_ptr, LogLevel::SYSFATAL, fmt, ##__VA_ARGS__); \
    } while (0)

#define LOG_SYSERR_FMT(logger_ptr, fmt, ...) \
    do { \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(LogLevel::SYSERR)) { \
            break; \
        } \
        LOG_FMT_HELPER(logger_ptr, LogLevel::SYSERR, fmt, ##__VA_ARGS__); \
    } while (0)

#define LOG_FATAL_FMT(logger_ptr, fmt, ...) \
    do { \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(LogLevel::FATAL)) { \
            break; \
        } \
        LOG_FMT_HELPER(logger_ptr, LogLevel::FATAL, fmt, ##__VA_ARGS__); \
    } while (0)

#define LOG_ERROR_FMT(logger_ptr, fmt, ...) \
    do {                                \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(LogLevel::ERROR)) { \
            break; \
        } \
        LOG_FMT_HELPER(logger_ptr, LogLevel::ERROR, fmt, ##__VA_ARGS__); \
    } while (0)

#define LOG_WARN_FMT(logger_ptr, fmt, ...) \
    do { \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(LogLevel::WARN)) { \
            break; \
        } \
        LOG_FMT_HELPER(logger_ptr, LogLevel::WARN, fmt, ##__VA_ARGS__); \
    } while (0)

#define LOG_TRACE_FMT(logger_ptr, fmt, ...) \
    do { \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(LogLevel::TRACE)) { \
            break; \
        } \
        LOG_FMT_HELPER(logger_ptr, LogLevel::TRACE, fmt, ##__VA_ARGS__); \
    } while (0)

#define LOG_INFO_FMT(logger_ptr, fmt, ...) \
    do { \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(LogLevel::INFO)) { \
            break; \
        } \
        LOG_FMT_HELPER(logger_ptr, LogLevel::INFO, fmt, ##__VA_ARGS__); \
    } while (0)

#define LOG_DEBUG_FMT(logger_ptr, fmt, ...) \
    do { \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(LogLevel::DEBUG)) { \
            break; \
        } \
        LOG_FMT_HELPER(logger_ptr, LogLevel::DEBUG, fmt, ##__VA_ARGS__); \
    } while (0)

// --- 零成本 无参数日志宏 ---

#define LOG_SYSFATAL(logger_ptr) \
    do { \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(LogLevel::SYSFATAL)) { \
            break; \
        } \
        LOG_HELPER(logger_ptr, LogLevel::SYSFATAL); \
    } while (0)

#define LOG_SYSERR(logger_ptr) \
    do { \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(LogLevel::SYSERR)) { \
            break; \
        } \
        LOG_HELPER(logger_ptr, LogLevel::SYSERR); \
    } while (0)

#define LOG_FATAL(logger_ptr) \
    do { \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(LogLevel::FATAL)) { \
            break; \
        } \
        LOG_HELPER(logger_ptr, LogLevel::FATAL); \
    } while (0)

#define LOG_ERROR(logger_ptr) \
    do { \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(LogLevel::ERROR)) { \
            break; \
        } \
        LOG_HELPER(logger_ptr, LogLevel::ERROR); \
    } while (0)

#define LOG_WARN(logger_ptr) \
    do { \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(LogLevel::WARN)) { \
            break; \
        } \
        LOG_HELPER(logger_ptr, LogLevel::WARN); \
    } while (0)

#define LOG_TRACE(logger_ptr) \
    do { \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(LogLevel::TRACE)) { \
            break; \
        } \
        LOG_HELPER(logger_ptr, LogLevel::TRACE); \
    } while (0)

#define LOG_INFO(logger_ptr) \
    do { \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(LogLevel::INFO)) { \
            break; \
        } \
        LOG_HELPER(logger_ptr, LogLevel::INFO); \
    } while (0)

#define LOG_DEBUG(logger_ptr) \
    do { \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(LogLevel::DEBUG)) { \
            break; \
        } \
        LOG_HELPER(logger_ptr, LogLevel::DEBUG); \
    } while (0)
#endif