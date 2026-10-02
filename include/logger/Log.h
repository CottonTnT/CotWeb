#pragma once
#include "Logger.h"
#include "LoggerManager.h"

namespace X::detail {

template <typename... Args>
auto format(std::format_string<Args...> fmt, Args&&... args)
    -> InlineBuffer<128_kb>
{
    // 1. 准备栈上固定的内联缓冲区
    auto stkBuffer = std::array<char, 1_kb> { };

    // 2. 第一次尝试：直接格式化到栈缓冲区中
    // std::format_to_n 会返回写出的字符数以及格式化所需的总字符数 (total_size)
    const auto [_, total_size] = std::format_to_n(
        stkBuffer.data(),
        stkBuffer.size(),
        fmt,
        std::forward<Args>(args)...);

    // 3. Fast Path：栈缓冲区能够完全容纳格式化后的字符串

    auto inlineBuffer = InlineBuffer<128_kb> { };
    if (total_size <= stkBuffer.size()) {
        inlineBuffer.append(stkBuffer.data(), static_cast<std::streamsize>(total_size));
        return inlineBuffer;
    }

    // 4. Slow Path：栈空间不足发生溢出，在堆上开辟精准大小的缓冲区进行二次格式化
    auto overflow_buffer = std::vector<char>(total_size);
    // std::format_to 保证精准写入 overflow_buffer
    std::format_to(overflow_buffer.data(), fmt, std::forward<Args>(args)...);
    inlineBuffer.append(overflow_buffer.data(), static_cast<std::streamsize>(total_size));

    return inlineBuffer;
}

template <typename... Args>
inline auto LogFMT(const Logger& logger,
    const X::LogLevel loglevel,
    const u32 lineNumber,
    const std::format_string<Args...> fmt,
    const Args&&... args)
    -> void
{
    auto messageBuffer = format(fmt, std::forward<Args>(args)...);

    auto recordView = LogRecordView::of(loglevel,
        logger.getLoggerName(),
        std::chrono::system_clock::now().time_since_epoch(),
        std::chrono::system_clock::now(),
        1,
        "thread_name",
        __FILE__,
        messageBuffer.view(),
        lineNumber);

    logger.log(recordView);
}

}

// 宏定义：现在宏负责捕获位置信息并传递给模板函数
#define LOG_FMT_HELPER(logger_ptr, loglevel, fmt, ...) \
    do {                                               \
        X::detail::LogFMT(*(logger_ptr),               \
            loglevel,                                  \
            __LINE__,                                  \
            __FILE__,                                  \
            fmt,                                       \
            ##__VA_ARGS__);                            \
    } while (0)

// 假设 logger_ptr 指向的 Logger 对象有一个 isLevelEnabled(X::LogLevel) 方法
// 用于快速判断是否需要记录日志。

// --- 带有格式化参数的宏 ---

#define LOG_SYSFATAL_FMT(logger_ptr, fmt, ...)                                       \
    do {                                                                             \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(X::LogLevel::SYSFATAL)) { \
            break;                                                                   \
        }                                                                            \
        LOG_FMT_HELPER(logger_ptr, X::LogLevel::SYSFATAL, fmt, ##__VA_ARGS__);       \
    } while (0)

#define LOG_SYSERR_FMT(logger_ptr, fmt, ...)                                       \
    do {                                                                           \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(X::LogLevel::SYSERR)) { \
            break;                                                                 \
        }                                                                          \
        LOG_FMT_HELPER(logger_ptr, X::LogLevel::SYSERR, fmt, ##__VA_ARGS__);       \
    } while (0)

#define LOG_FATAL_FMT(logger_ptr, fmt, ...)                                       \
    do {                                                                          \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(X::LogLevel::FATAL)) { \
            break;                                                                \
        }                                                                         \
        LOG_FMT_HELPER(logger_ptr, X::LogLevel::FATAL, fmt, ##__VA_ARGS__);       \
    } while (0)

#define LOG_ERROR_FMT(logger_ptr, fmt, ...)                                       \
    do {                                                                          \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(X::LogLevel::ERROR)) { \
            break;                                                                \
        }                                                                         \
        LOG_FMT_HELPER(logger_ptr, X::LogLevel::ERROR, fmt, ##__VA_ARGS__);       \
    } while (0)

#define LOG_WARN_FMT(logger_ptr, fmt, ...)                                       \
    do {                                                                         \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(X::LogLevel::WARN)) { \
            break;                                                               \
        }                                                                        \
        LOG_FMT_HELPER(logger_ptr, X::LogLevel::WARN, fmt, ##__VA_ARGS__);       \
    } while (0)

#define LOG_TRACE_FMT(logger_ptr, fmt, ...)                                       \
    do {                                                                          \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(X::LogLevel::TRACE)) { \
            break;                                                                \
        }                                                                         \
        LOG_FMT_HELPER(logger_ptr, X::LogLevel::TRACE, fmt, ##__VA_ARGS__);       \
    } while (0)

#define LOG_INFO_FMT(logger_ptr, fmt, ...)                                       \
    do {                                                                         \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(X::LogLevel::INFO)) { \
            break;                                                               \
        }                                                                        \
        LOG_FMT_HELPER(logger_ptr, X::LogLevel::INFO, fmt, ##__VA_ARGS__);       \
    } while (0)

#define LOG_DEBUG_FMT(logger_ptr, fmt, ...)                                       \
    do {                                                                          \
        if (!(logger_ptr) || !(logger_ptr)->isLevelEnabled(X::LogLevel::DEBUG)) { \
            break;                                                                \
        }                                                                         \
        LOG_FMT_HELPER(logger_ptr, X::LogLevel::DEBUG, fmt, ##__VA_ARGS__);       \
    } while (0)

static auto log = GET_ROOT_LOGGER();

#define EASY_SYSFATAL(fmt, ...) LOG_SYSFATAL_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_SYSERR(fmt, ...) LOG_SYSERR_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_FATAL(fmt, ...) LOG_FATAL_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_ERROR(fmt, ...) LOG_ERROR_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_WARN(fmt, ...) LOG_WARN_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_TRACE(fmt, ...) LOG_TRACE_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_INFO(fmt, ...) LOG_INFO_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_DEBUG(fmt, ...) LOG_DEBUG_FMT(log, fmt, ##__VA_ARGS__)
#define EASY_ALL(fmt, ...) LOG_ALL_FMT(log, fmt, ##__VA_ARGS__)
