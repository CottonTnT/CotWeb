#pragma once

#include <memory>
#include <string_view>

#include "LogAppender.h"
#include "LogLevel.h"

namespace X {

class Logger;
class LoggerManager;
class LogAppender;

class Logger;
using LoggerSPtr = std::shared_ptr<Logger>;

class Logger final {

private:
    class Impl;
    friend class LogService;

    std::unique_ptr<Impl> impl_;

public:
    explicit Logger(std::string name, LoggerSPtr root = nullptr);
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    [[nodiscard]] auto getLogLevel() const;
    void setLogLevel(LogLevel level) noexcept;

    [[nodiscard]] auto getLoggerName() const -> std::string_view;

    void setFormatPattern(std::string pattern);
    [[nodiscard]] auto getFormatPattern() const noexcept
        -> std::string_view;

    void addAppender(AppenderPtr appender);
    void removeAppender(AppenderPtr appender);
    void clearAppender();
};

}
