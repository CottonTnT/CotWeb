
#ifndef LOGAPPENDER_H
#define LOGAPPENDER_H

#include "AppenderFacade.h"
#include "LogFormatter.h"


class LogFormatter;
class LogEvent;

template <typename T>
concept IsAppenderImpl = requires(T x, LogFormatter y, LogEvent z) {
    x.log(y, z);
};

/**
 * @brief thread-safe, actually a proxy of the concrete appender
 */

template <typename Impl>
requires IsAppenderImpl<Impl>
class AppenderProxy :  public AppenderFacade {
public:
    AppenderProxy()
        : formatter_ {LogFormatter{}}
        {}
    AppenderProxy(const AppenderProxy&)            = delete;
    AppenderProxy(AppenderProxy&&)                 = delete;
    auto operator=(const AppenderProxy&) -> AppenderProxy& = delete;
    auto operator=(AppenderProxy&&) -> AppenderProxy&      = delete;

    template <typename... Ts>
    explicit AppenderProxy(LogFormatter fmter, Ts&&... ts)
        : formatter_(std::move(fmter))
        , impl_(std::forward<Ts>(ts)...)
    {
    }

    template <typename... Ts>
    explicit AppenderProxy(Ts&&... ts)
        : formatter_{ LogFormatter{} }
        , impl_(std::forward<Ts>(ts)...)
    {
    }

    [[nodiscard]] auto GetFormater() const
        -> const LogFormatter&
    {
        return formatter_;
    }

    auto SetFormatPattern(LogFormatter formatter)
        -> void
    {
        formatter_ = std::move(formatter);
    }

    void log(const LogEvent& event) override
    {
        impl_.log(formatter_, event);
    }

     ~AppenderProxy() override = default;

private:
    LogFormatter formatter_;
    Impl impl_;
};

#endif