#pragma once

#include "logger/LogAppender.h"

namespace X::detail {

// 1. 定义一个 Concept，约束 Impl 必须实现指定的三个方法
template <typename T>
concept LogAppenderConcept = requires(T impl, std::string_view event) {
    { impl.append(event) };
    { impl.flush() };
    { impl.sync() };
};

/**
 * @brief thread-safe, actually a proxy of the concrete appender
 */

template <LogAppenderConcept Impl>
class AppenderProxy : public LogAppender {
public:
    AppenderProxy() = default;
    AppenderProxy(const AppenderProxy&) = delete;
    AppenderProxy(AppenderProxy&&) = delete;
    auto operator=(const AppenderProxy&) -> AppenderProxy& = delete;
    auto operator=(AppenderProxy&&) -> AppenderProxy& = delete;

    template <typename... Ts>
        requires std::is_constructible_v<Impl, Ts...>
    explicit AppenderProxy(Ts&&... ts)
        : impl_(std::forward<Ts>(ts)...)
    {
    }

    void append(std::string_view event) override
    {
        impl_.append(event);
    }

    void flush() override
    {
        impl_.flush();
    }
    void sync() override
    {
        impl_.sync();
    }

    ~AppenderProxy() override = default;

private:
    Impl impl_;
};
}
