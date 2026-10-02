#pragma once
#include "common/XType.h"
#include "logger/LogLevel.h"

/**
 * @brief 日志事件，用于记录日志现场，比如该日志的级别，文件名/行号，日志消息，线程/协程号，所属日志器名称等。
 */

namespace X::detail {

struct LogRecordView {

    const std::string_view loggerName_;
    const LogLevel logLevel_;
    const std::chrono::system_clock::duration elapsed_;
    const std::chrono::system_clock::time_point timestamp_;
    const u64 threadId_;
    const std::string_view threadName_;
    const std::string_view filename_;
    const std::string_view custom_msg_;
    const u32 lineNumber_;

    LogRecordView(LogLevel level,
        std::string_view loggerName,
        std::chrono::system_clock::duration elapsed,
        std::chrono::system_clock::time_point timestamp,
        u64 threadId,
        std::string_view threadName,
        std::string_view filename,
        std::string_view custom_msg,
        u32 lineNumber)
        : logLevel_(level)
        , loggerName_(loggerName)
        , elapsed_(elapsed)
        , timestamp_(timestamp)
        , threadId_(threadId)
        , threadName_(threadName)
        , filename_(filename)
        , custom_msg_(custom_msg)
        , lineNumber_(lineNumber)
    {
    }

public:
    LogRecordView(const LogRecordView&) = default;
    auto operator=(const LogRecordView&) -> LogRecordView& = delete;
    LogRecordView(LogRecordView&&) = default;
    auto operator=(LogRecordView&&) -> LogRecordView& = delete;

    static auto of(LogLevel level,
        std::string_view loggerName,
        std::chrono::system_clock::duration elapsed,
        std::chrono::system_clock::time_point timestamp,
        u64 threadId,
        std::string_view threadName,
        std::string_view filename,
        std::string_view custom_msg,
        u32 lineNumber) -> LogRecordView
    {
        return LogRecordView { level, loggerName, elapsed, timestamp, threadId, threadName, filename, custom_msg, lineNumber };
    }
};

}