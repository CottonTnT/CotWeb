#pragma once
#include "InlineBuffer.hpp"
#include "Logger.h"
// #include "LoggerManager.h"

#include <format>
#include <source_location>
#include <type_traits>

namespace X {

namespace detail {

    template <typename... Args>
    struct FmtStrWithSrcLoc {
        std::format_string<Args...> fmt;
        std::source_location loc;

        // 利用隐式转换与默认实参，在传入字符串字面量时顺便捕获调用点位置
        template <typename T>
            requires std::convertible_to<const T&, std::string_view>
        consteval FmtStrWithSrcLoc(const T& s,
            std::source_location loc = std::source_location::current())
            : fmt(s)
            , loc(loc)
        {
        }
    };

    constexpr int USER_CUSTOME_MESSAGE_INLINE_BUFFER_SIZE = 256;
    using UserCustomMessageBuffer = InlineBuffer<USER_CUSTOME_MESSAGE_INLINE_BUFFER_SIZE>;

    template <typename... Args>
    auto format(std::format_string<Args...> fmt, Args&&... args)
        -> UserCustomMessageBuffer
    {
        // 1. 准备栈上固定的内联缓冲区
        auto stkBuffer = std::array<char, 1024> { };

        // 2. 第一次尝试：直接格式化到栈缓冲区中
        // std::format_to_n 会返回写出的字符数以及格式化所需的总字符数 (total_size)
        const auto [_, total_size] = std::format_to_n(
            stkBuffer.data(),
            stkBuffer.size(),
            fmt,
            std::forward<Args>(args)...);

        // 3. Fast Path：栈缓冲区能够完全容纳格式化后的字符串

        auto inlineBuffer = UserCustomMessageBuffer { };
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

}

class LogService {
private:
    static void logImpl(const LoggerSPtr& logger, LogLevel level, std::string_view message, std::source_location loc);

    static auto getRootLogger() noexcept
        -> LoggerSPtr;

public:
    template <typename... Args>
    static void debug(const LoggerSPtr& logger, detail::FmtStrWithSrcLoc<std::type_identity_t<Args>...> fmt, Args&&... args);

    template <typename... Args>
    static void info(const LoggerSPtr& logger, detail::FmtStrWithSrcLoc<std::type_identity_t<Args>...> fmt, Args&&... args);

    template <typename... Args>
    static void error(const LoggerSPtr& logger, detail::FmtStrWithSrcLoc<std::type_identity_t<Args>...> fmt, Args&&... args);

    template <typename... Args>
    static void debug(detail::FmtStrWithSrcLoc<std::type_identity_t<Args>...> fmt, Args&&... args);

    template <typename... Args>
    static void info(detail::FmtStrWithSrcLoc<std::type_identity_t<Args>...> fmt, Args&&... args);

    template <typename... Args>
    static void error(detail::FmtStrWithSrcLoc<std::type_identity_t<Args>...> fmt, Args&&... args);
};

template <typename... Args>
void LogService::debug(const LoggerSPtr& logger,
    detail::FmtStrWithSrcLoc<std::type_identity_t<Args>...> fmt,
    Args&&... args)
{
    logImpl(logger,
        LogLevel::DEBUG,
        detail::format(fmt.fmt, std::forward<Args>(args)...).view(),
        fmt.loc);
}

template <typename... Args>
void LogService::debug(detail::FmtStrWithSrcLoc<std::type_identity_t<Args>...> fmt,
    Args&&... args)
{
    logImpl(getRootLogger(),
        LogLevel::DEBUG,
        detail::format(fmt.fmt, std::forward<Args>(args)...).view(),
        fmt.loc);
}

}
