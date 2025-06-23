#ifndef COT_LOGEVENT_H
#define COT_LOGEVENT_H
#include "common.h"
#include "loglevel.h"
#include <cstdarg>
#include <sstream>

namespace LogT {

/**
 * @brief 日志事件，用于记录日志现场，比如该日志的级别，文件名/行号，日志消息，线程/协程号，所属日志器名称等。
 */

class Event {
public:
    /**
     * @brief 构造函数
     * @param[in] logger_name 日志器名称
     * @param[in] level 日志级别
     * @param[in] filename 文件名
     * @param[in] line 文件行号
     * @param[in] elapse 程序启动依赖的耗时(毫秒)
     * @param[in] thread_id 线程id
     * @param[in] fiber_id 协程id
     * @param[in] time 日志事件(UTC秒)
     * @param[in] thread_name 线程名称
     */
    Event(std::string_view logger_name,
          Level loglevel,
          uint32_t line_number,
          std::string_view filename,
          uint32_t elapse,
          uint32_t thread_id,
          std::string_view thread_name,
          uint32_t co_id,
          time_t timestamp);

    auto GetFilename() const
        -> std::string_view { return filename_; }

    auto GetLine() const
        -> uint32_t { return line_number_; }
    auto GetElapse() const
        -> uint32_t { return elapse_; }
    auto GetThreadId() const
        -> uint32_t { return thread_id_; }
    auto GetFiberId() const
        -> uint32_t { return co_id_; }
    auto GetTime() const
        -> uint64_t { return timestamp_; }

    auto GetThreadName() const
        -> std::string_view { return thread_name_; }
    auto GetLoggerName() const
        -> std::string_view { return logger_name_; }

    auto GetContent() const
        -> std::string { return content_stream_.str(); }

    auto GetSS()
        -> std::stringstream& { return content_stream_; }

    auto GetLevel() const
        -> Level { return loglevel_; }

    /**
     * @todo use template vargs to format
     */
    void Printf(const char* fmt, ...);

private:
    void VPrintf(const char* fmt, va_list al);

private:
    std::string logger_name_;
    Level loglevel_ {Level::DEBUG};
    uint32_t line_number_ {};
    std::string filename_;
    uint32_t elapse_ {};
    uint32_t thread_id_ {};
    std::string thread_name_;
    uint32_t co_id_ {};
    // std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> time_stamp_;
    std::time_t timestamp_ {};
    std::stringstream content_stream_;
};

class Logger;

/**
 * @brief 利用RAII输出日志
 */
class LogGuard {
public:
    LogGuard(const LogGuard&)            = default;
    LogGuard(LogGuard&&)                 = default;
    LogGuard& operator=(const LogGuard&) = default;
    LogGuard& operator=(LogGuard&&)      = default;
    LogGuard(Sptr<Logger> logger, Sptr<Event> event);
    auto GetLogEvent() const
        -> Sptr<Event> { return event_; }

    ~LogGuard();
private:
    Sptr<Logger> logger_;
    Sptr<Event> event_;
};

}

#endif