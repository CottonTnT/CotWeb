#pragma once

#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <tuple>

#include "logger/LogAppender.h"
#include "logger/LogLevel.h"
#include "logger/Logger.h"

#include "LogFormatter.h"
#include "LogRecordView.h"

namespace X {

namespace detail {
    constexpr std::string_view DEFAULT_LOG_PATTERN = "%d{%Y-%m-%d %H:%M:%S} [%rms] %t%T%N%T%F%T[%p]%T[%c]%T[%f:%l]%T%T%m%n";
} // namespace detail

class Logger::Impl {

private:
    std::string loggerName_;
    std::atomic<LogLevel> logLevel_;

    mutable std::shared_mutex mtx_;
    std::shared_ptr<const detail::LogFormatter> logFomatter_;
    std::vector<AppenderPtr> logAppenderList_;

    LoggerSPtr rootLogger_;

public:
    explicit Impl(std::string name, LoggerSPtr root);
    [[nodiscard]] auto shouldLog(LogLevel level) const noexcept
        -> bool;
    void submit(const detail::LogRecordView& record) noexcept;
    void setLevel(LogLevel level) noexcept;
    [[nodiscard]] auto getLogLevel() const noexcept -> LogLevel;
    void setLogLevel(LogLevel level) noexcept;
    [[nodiscard]] auto getName() const noexcept -> std::string_view;
    void setFormatter(detail::LogFormatterPtr formatter);
    [[nodiscard]] auto getFormatter() const noexcept
        -> detail::LogFormatterPtr;
    void addAppender(AppenderPtr appender);
    void removeAppender(const AppenderPtr& appender);
    void clearAppenders();

    void flush();
    void sync();

    void setRoot(const LoggerSPtr& root) noexcept;
};

inline Logger::Impl::Impl(std::string name, LoggerSPtr root)
    : loggerName_ { std::move(name) }
    , logLevel_ { LogLevel::DEBUG }
    , logFomatter_ { std::make_shared<const detail::LogFormatter>(std::string { detail::DEFAULT_LOG_PATTERN }) }
    , rootLogger_ { std::move(root) }

{
    if (loggerName_.empty()) {
        throw std::invalid_argument("logger name cannot be empty");
    }
}

inline auto Logger::Impl::shouldLog(LogLevel level) const noexcept
    -> bool
{
    if (level < getLogLevel()) {
        return false;
    }

    {
        auto _ = std::shared_lock<std::shared_mutex> { mtx_ };
        if (!logAppenderList_.empty()) {
            return true;
        }
    }
    return rootLogger_ != nullptr && rootLogger_->impl_->shouldLog(level);
}

inline void Logger::Impl::submit(const detail::LogRecordView& record) noexcept
{
    if (record.logLevel_ < getLogLevel()) {
        return;
    }

    std::shared_lock<std::shared_mutex> lock(mtx_);
    if (logAppenderList_.empty()) {
        lock.unlock();
        if (rootLogger_ != nullptr) {
            rootLogger_->impl_->submit(record);
        }
        return;
    }
    // InlineBuffer，它遇到长日志的时候，可能会使用堆内存
    // vector开辟堆内存的时候，会有极小概率抛bad_alloc异常，这里不能让异常逃逸出去
    try {
        auto buf = logFomatter_->format(record);
        for (const auto& appender : logAppenderList_) {
            appender->append(buf.view());
        }
    }
    // 如果try失败，我们往标准错误里打一条提示
    catch (...) {
        constexpr std::string_view message = "pans logger: record formatting failed\n";
        std::ignore = std::fwrite(message.data(), 1, message.size(), stderr);
    }
}

inline auto Logger::Impl::getLogLevel() const noexcept
    -> LogLevel
{
    return logLevel_.load(std::memory_order_acquire);
}

inline void Logger::Impl::setLogLevel(LogLevel level) noexcept
{
    logLevel_.store(level, std::memory_order_release);
}

inline void Logger::Impl::setFormatter(detail::LogFormatterPtr formatter)
{
    assert(formatter != nullptr);
    auto _ = std::unique_lock<std::shared_mutex>(mtx_);
}
[[nodiscard]] inline auto Logger::Impl::getFormatter() const noexcept
    -> detail::LogFormatterPtr
{
    auto _ = std::shared_lock<std::shared_mutex>(mtx_);
    return logFomatter_;
}

inline void Logger::Impl::addAppender(AppenderPtr appender)
{
    // ASSERT_RETNONE2(appender != nullptr, "logger appender cannot be null");
    auto _ = std::unique_lock<std::shared_mutex>(mtx_);
    logAppenderList_.push_back(std::move(appender));
}

inline void Logger::Impl::removeAppender(const AppenderPtr& appender)
{
    auto _ = std::unique_lock<std::shared_mutex>(mtx_);
    std::erase(logAppenderList_, appender);
}

inline void Logger::Impl::clearAppenders()
{
    auto _ = std::unique_lock<std::shared_mutex>(mtx_);
    logAppenderList_.clear();
}

inline void Logger::Impl::flush()
{
    auto lock = std::shared_lock<std::shared_mutex>(mtx_);
    if (logAppenderList_.empty()) {
        lock.unlock();
        if (rootLogger_ != nullptr) {
            rootLogger_->impl_->flush();
        }
        return;
    }

    for (const AppenderPtr& appender : logAppenderList_) {
        appender->flush();
    }
}

inline void Logger::Impl::sync()
{
    auto lock = std::shared_lock<std::shared_mutex>(mtx_);
    if (logAppenderList_.empty()) {
        lock.unlock();
        if (rootLogger_ != nullptr) {
            rootLogger_->impl_->sync();
        }
        return;
    }

    for (const AppenderPtr& appender : logAppenderList_) {
        appender->sync();
    }
}

inline void Logger::Impl::setRoot(const LoggerSPtr& root) noexcept
{
    rootLogger_ = root;
}

namespace detail {
    /**
     * LoggerAccess本质上是一个内部权限访问的桥梁，
     * 用于在不扩大Logger公共API的情况下，访问其私有实现
     */
    class LoggerAccess final {
    public:
        static void Submit(Logger& logger, const LogRecordView& record) noexcept;
        static void SetRoot(Logger& logger, const LoggerSPtr& root) noexcept;
    };

} // namespace detail
} // namespace X
