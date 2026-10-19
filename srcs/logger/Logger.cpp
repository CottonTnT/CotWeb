
#include "logger/Logger.h"
#include "../common/Singleton.hpp"
#include "LogFormatter.h"
#include "LoggerImpl.hpp"
#include "logger/LogAppender.h"
#include "logger/LogLevel.h"

#include <memory>
#include <unordered_map>

namespace X {

/* ======================== Logger ======================== */

/**
 * @brief 日志器，用于输出日志。这个类是直接与用户进行交互的类，提供 log 方法用于输出日志事件。不带root logger。
 *  Logger的实现包含了日志级别，日志器名称，创建时间，以及一个 LogAppender 数组，
 * 日志事件由 log 方法输出，log 方法首先判断日志级别是否达到本 Logger 的级别要求，
 * 是则将日志传给各个 LogAppender 进行输出，否则抛弃这条日志。
 */

Logger::Logger(std::string name, LoggerSPtr root)
    : impl_ {
        std::make_unique<Impl>(std::move(name), std::move(root))
    }
{
}

void Logger::addAppender(const AppenderPtr appender)
{
    impl_->addAppender(std::move(appender));
}

void Logger::removeAppender(const AppenderPtr appender)
{
    impl_->removeAppender(appender);
}

void Logger::clearAppender()
{
    impl_->clearAppenders();
}

auto Logger::getLogLevel() const
{
    return impl_->getLogLevel();
}

void Logger::setLogLevel(LogLevel level) noexcept
{
    impl_->setLevel(level);
}

auto Logger::getLoggerName() const
    -> std::string_view
{
    return impl_->getName();
}

void Logger::setFormatPattern(std::string pattern)
{

    auto newFormatter = std::make_shared<detail::LogFormatter>(std::move(pattern));
    impl_->setFormatter(newFormatter);
}

[[nodiscard]] auto Logger::getFormatPattern() const noexcept
    -> std::string_view
{
}

} // namespace X
