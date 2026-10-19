#include "logger/LogService.hpp"
#include "LogManager.hpp"
#include "LogRecordView.h"
#include "LoggerImpl.hpp"

namespace X {

void LogService::logImpl(const LoggerSPtr& logger, LogLevel level, std::string_view message, std::source_location loc)
{

    auto recordView = detail::LogRecordView {
        .loggerName_ = logger->getLoggerName(),
        .logLevel_ = level,
        .elapsed_ = std::chrono::system_clock::now().time_since_epoch(),
        .timestamp_ = std::chrono::system_clock::now(),
        .threadId_ = 1,
        .threadName_ = "thread_name",
        .filename_ = loc.file_name(),
        .customeMsg_ = message,
        .lineNumber_ = loc.line()
    };
    logger->impl_->submit(recordView);
}

auto LogService::getRootLogger() noexcept
    -> LoggerSPtr
{
    return detail::LoggerManager::getInstance().getRootLogger();
}

} // namespace X