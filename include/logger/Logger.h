#pragma once

#include <chrono>
#include <list>
#include <memory>
#include <utility>

#include "InlineBuffer.h"
#include "LogLevel.h"
#include "LogRecordView.h"
#include "common/XType.h"
#include "logger/AppenderFacade.h"

namespace X {

class Logger;
class LoggerManager;
class AppenderFacade;

/**
 * @brief 日志器，用于输出日志。这个类是直接与用户进行交互的类，提供 log 方法用于输出日志事件。不带root logger。
 *  Logger的实现包含了日志级别，日志器名称，创建时间，以及一个 LogAppender 数组，
 * 日志事件由 log 方法输出，log 方法首先判断日志级别是否达到本 Logger 的级别要求，
 * 是则将日志传给各个 LogAppender 进行输出，否则抛弃这条日志。
 */
class Logger {
public:
    class Impl;

    explicit Logger(std::string name = "root");

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    [[nodiscard]] bool shouldLog(LogLevel level) const noexcept;
    void log(const LogRecordView& event) const;

    void addAppender(Sptr<AppenderFacade> appender);
    void delAppender(Sptr<AppenderFacade> appender);
    void clearAppender();

    auto getLoggerName() const& -> const std::string& { return name_; }

    auto getLoggerName() && -> std::string { return std::move(name_); }
    auto getLogLevel() const
        -> X::LogLevel { return level_; }

    auto setLogLevel(LogLevel level)
        -> void { level_ = level; }

    void flush();
    void sync();

private:
    std::unique_ptr<Impl> impl_;
};

using LoggerPtr = std::shared_ptr<Logger>;

}
