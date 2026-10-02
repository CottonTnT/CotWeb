
#include "logger/Logger.h"
#include "../common/Singleton.hpp"
#include "logger/LogLevel.h"

#include <algorithm>
#include <cstdlib>
#include <unordered_map>

namespace X {

/* ======================== Logger ======================== */

/**
 * @brief 日志器，用于输出日志。这个类是直接与用户进行交互的类，提供 log 方法用于输出日志事件。不带root logger。
 *  Logger的实现包含了日志级别，日志器名称，创建时间，以及一个 LogAppender 数组，
 * 日志事件由 log 方法输出，log 方法首先判断日志级别是否达到本 Logger 的级别要求，
 * 是则将日志传给各个 LogAppender 进行输出，否则抛弃这条日志。
 */
class Logger : public std::enable_shared_from_this<Logger> {
private:
    std::string name_; // 日志器名称
    LogLevel level_ = LogLevel::ALL; // 日志级别
    std::list<Sptr<AppenderFacade>> appenders_; // Appender集合

public:
    explicit Logger(std::string name = "root");

    void log(const LogRecordView& event) const;

    void addAppender(Sptr<AppenderFacade> appender);
    void delAppender(Sptr<AppenderFacade> appender);
    void clearAppender();

    auto getLoggerName() const& -> const std::string& { return name_; }

    auto getLoggerName() && -> std::string { return std::move(name_); }
    auto getLogLevel() const
        -> X::LogLevel { return level_; }
    auto isLevelEnabled(X::LogLevel level) const
        -> bool
    {
        return level >= level_;
    }

    auto setLogLevel(LogLevel level)
        -> void { level_ = level; }
};

Logger::Logger(std::string name)
    : name_(std::move(name))
{
}

void Logger::addAppender(Sptr<AppenderFacade> appender)
{
    appenders_.push_back(appender);
}

void Logger::delAppender(Sptr<AppenderFacade> appender)
{
    for (auto it = std::begin(appenders_); it != std::end(appenders_); it++) {
        if (*it == appender) {
            appenders_.erase(it);
            break;
        }
    }
}

void Logger::clearAppender()
{
    appenders_.clear();
}

void Logger::log(const LogRecordView& event) const
{

    // if (event.getLevel() < level_) {
    //     return; // 不算错误，直接忽略
    // }
    // if (isLevelEnabled(event.getLevel())) {
    //     std::ranges::for_each(appenders_,
    //         [&event](Sptr<AppenderFacade> appender) {
    //             appender->log(event);
    //         });
    // }
    // if (event.getLevel() == LogLevel::SYSFATAL) {
    //     // todo: bug here, flush all appenders before exit
    //     ::exit(EXIT_FAILURE);
    // }
}

} // namespace X

namespace X::detail {

class LoggerManager final : public SingletonBean<LoggerManager> {

public:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, LoggerPtr> m_loggers;
    LoggerPtr m_root;

    LoggerManager(BeanKey /*unused*/)
        : m_root(std::make_shared<Logger>("root"))
    {
        // m_root->addAppender(MakeStdoutAppender());
        m_loggers.emplace("root", m_root);
    }

    [[nodiscard]] auto getRoot() const noexcept
        -> LoggerPtr
    {
        return m_root;
    }

    [[nodiscard]] auto getLogger(std::string_view name)
        -> LoggerPtr
    {
        // ASSERT_RETVAL2(!name.empty(), nullptr, "logger name cannot be empty");

        std::lock_guard<std::mutex> lock(m_mutex);
        const auto iterator = m_loggers.find(std::string(name));
        if (iterator != m_loggers.end()) {
            return iterator->second;
        }

        auto logger = std::make_shared<Logger>(std::string(name));
        // detail::LoggerAccess::SetRoot(*logger, m_root);
        // m_loggers.emplace(logger->getName(), logger);
        return logger;
    }
};

}
