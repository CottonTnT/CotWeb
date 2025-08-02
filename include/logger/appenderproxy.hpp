
#ifndef LOGAPPENDER_H
#define LOGAPPENDER_H

#include "common/alias.h"
#include "appenderbase.h"
#include "logformatter.h"
#include "common/mutex.h"
#include <memory>


namespace LogT {

class LogFormatter;
class Event;

template <typename T>
concept IsAppenderImpl = requires(T x, Sptr<LogFormatter> y, Sptr<Event> z) {
    x.Log(y, z);
};

/**
 * @brief thread-safe, actually a proxy of the concrete appender
 */

template <typename Impl>
requires IsAppenderImpl<Impl>
class Appender :  public AppenderProxyBase {
public:
    using MutexType = Cot::SpinMutex;

    Appender()
        : formatter_ {new LogFormatter}
        {}
    Appender(const Appender&)            = delete;
    Appender(Appender&&)                 = delete;
    Appender& operator=(const Appender&) = delete;
    Appender& operator=(Appender&&)      = delete;

    template <typename... Ts>
    explicit Appender(Sptr<LogFormatter> format_pattern, Ts&&... ts)
        : formatter_(format_pattern)
        , impl_(std::forward<Ts>(ts)...)
    {
    }

    template <typename... Ts>
    explicit Appender(Ts&&... ts)
        : formatter_(std::make_shared<LogFormatter>())
        , impl_(std::forward<Ts>(ts)...)
    {
    }

    auto GetFormatPattern() const
        -> Sptr<LogFormatter>
    {
        auto _ = Cot::LockGuard<MutexType>(mtx_);
        return formatter_;
    }

    auto SetFormatPattern(Sptr<LogFormatter> formatter)
        -> void
    {
        auto _ = Cot::LockGuard<MutexType>(mtx_);
        formatter_ = formatter;
    }

    void Log(Sptr<Event> event) override
    {
        auto _ = Cot::LockGuard<MutexType>(mtx_);
        impl_.Log(formatter_, event);
    }

     ~Appender() override = default;

private:
    Sptr<LogFormatter> formatter_;
    Impl impl_;
    mutable MutexType mtx_;
};

#endif
} //namespace LogT