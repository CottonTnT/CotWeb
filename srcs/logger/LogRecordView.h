#pragma once
#include "common/XType.h"
#include "logger/LogLevel.h"
#include <chrono>

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
    const std::string_view customeMsg_;
    const u32 lineNumber_;

    // LogRecordView(LogLevel level,
    //     std::string_view loggerName,
    //     std::chrono::steady_clock::duration elapsed,
    //     std::chrono::system_clock::time_point timestamp,
    //     u64 threadId,
    //     std::string_view threadName,
    //     std::string_view filename,
    //     std::string_view custom_msg,
    //     u32 lineNumber)
    //     : loggerName_(loggerName)
    //     , logLevel_(level)
    //     , elapsed_(elapsed)
    //     , timestamp_(timestamp)
    //     , threadId_(threadId)
    //     , threadName_(threadName)
    //     , filename_(filename)
    //     , customeMsg_(custom_msg)
    //     , lineNumber_(lineNumber)
    // {
    // }
};

}