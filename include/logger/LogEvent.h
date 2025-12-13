#pragma once
#include "LogLevel.h"
#include <cstdarg>
#include <source_location>
#include <sstream>
#include <thread>
#include <utility>


/**
 * @brief 日志事件，用于记录日志现场，比如该日志的级别，文件名/行号，日志消息，线程/协程号，所属日志器名称等。
 */

class LogEvent {
public:
    LogEvent() = default;
    LogEvent(const LogEvent&)            = delete;
    LogEvent(LogEvent&&)                  noexcept = default;
    auto operator=(const LogEvent&) -> LogEvent& = delete;
    auto operator=(LogEvent&&) -> LogEvent&      = default;
    /**
     * @brief 构造函数
     * @param logger_name 日志器名称
     * @param level 日志级别
     * @param elapse 程序启动依赖的耗时(毫秒)
     * @param thread_id 线程id
     * @param thread_name 线程名称
     * @param time 日志事件(UTC秒)
     * @param co_id 协程id
     * @param source_loc 源码位置信息
     */
    LogEvent(std::string logger_name,
          LogLevel loglevel,
          uint32_t elapse,
          std::thread::id thread_id,
          std::string thread_name,
          uint32_t co_id,
          time_t timestamp,
          std::source_location source_loc);
    ~LogEvent() = default;

    auto getFilename() const
        -> std::string { return source_loc_.file_name(); }

    auto getFunctionName() const
        -> std::string 
    {
        return source_loc_.function_name();
    }
    auto getLine() const
        -> uint32_t { return source_loc_.line(); }
    auto getElapse() const
        -> uint32_t { return elapse_; }
    auto getThreadId() const
        -> std::thread::id { return thread_id_; }
    auto getFiberId() const
        -> uint32_t { return co_id_; }
    auto getTime() const
        -> std::time_t { return timestamp_; }

    auto getThreadName() const &
        -> std::string_view { return thread_name_; }
    auto getLoggerName() const &
        -> std::string_view { return logger_name_; }

    auto getContent() const &
        -> std::string { return custom_msg_.str(); }

    auto getSS()
        -> std::stringstream& { return custom_msg_; }

    auto getLevel() const
        -> LogLevel { return loglevel_; }

    template <typename... Args>
    void print(std::format_string<Args...> fmt, Args&&... args)
    {
        custom_msg_ << std::format(fmt, std::forward<Args>(args)...);
    }

private:
    std::string logger_name_;
    LogLevel loglevel_ ;
    uint32_t elapse_;
    std::thread::id thread_id_;
    std::string thread_name_;
    uint32_t co_id_ ;
    std::time_t timestamp_ ;
    std::stringstream custom_msg_;
    std::source_location source_loc_;
};
