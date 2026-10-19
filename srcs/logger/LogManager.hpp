#pragma once
#include "common/Singleton.hpp"
#include "logger/Logger.h"
#include <string_view>
#include <unordered_map>

namespace X::detail {

class LoggerManager final : public SingletonBean<LoggerManager> {

private:
    static constexpr std::string_view ROOT_LOGGER_NAME = "ROOT";

    mutable std::mutex mtx_;
    std::unordered_map<std::string, LoggerSPtr> loggerMap_;
    LoggerSPtr rootLogger_;

public:
    explicit LoggerManager(BeanKey /*unused*/)
        : rootLogger_(std::make_shared<Logger>(std::string { ROOT_LOGGER_NAME }))
    {
        rootLogger_->addAppender(X::LogAppender::makeStdoutAppender());
    }
    [[nodiscard]] auto getRootLogger() const noexcept
        -> LoggerSPtr
    {
        return rootLogger_;
    }

    [[nodiscard]] auto getLogger(std::string name)
        -> LoggerSPtr
    {
        // ASSERT_RETVAL2(!name.empty(), nullptr, "logger name cannot be empty");

        std::lock_guard<std::mutex> lock(mtx_);
        const auto iterator = loggerMap_.find(std::string(name));
        if (iterator != loggerMap_.end()) {
            return iterator->second;
        }

        auto logger = std::make_shared<Logger>(std::move(name), rootLogger_);
        loggerMap_.emplace(logger->getLoggerName(), logger);
        return logger;
    }
};

}