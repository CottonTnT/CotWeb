#ifndef __LOG_H__
#define __LOG_H__

#include <list>

#include "common/mutex.h"
#include "common/alias.h"
#include "logevent.h"
#include "loglevel.h"


/**
 * @brief 巧妙利用了宏，既做到了作用域的效果，也能继续 如 `COT_LOG_LEVEL(logger, level) << xxx`一样的输出
 */   
#define COT_LOG_LEVEL(logger, level) \
    if (true)                      \
        LogT::LogGuard(logger,      \
                      Sptr<LogT::Event>(new LogT::Event("nihao", level, __LINE__, __FILE__, 0, UtilT::GetThreadId(),"tname_placeholder", 0, time(0)))         \
    ).GetLogEvent()->GetSS()

#define COT_LOG_DEBUG(logger) COT_LOG_LEVEL(logger, LogT::Level::DEBUG)
#define COT_LOG_INFO(logger)  COT_LOG_LEVEL(logger, LogT::Level::INFO)
#define COT_LOG_WARN(logger)  COT_LOG_LEVEL(logger, LogT::Level::WARN)
#define COT_LOG_ERROR(logger) COT_LOG_LEVEL(logger, LogT::Level::ERROR)
#define COT_LOG_FATAL(logger) COT_LOG_LEVEL(logger, LogT::Level::FATAL)
#define COT_LOG_ALERT(logger) COT_LOG_LEVEL(logger, LogT::Level::ALERT)

// /**
//  * @brief 使用C printf方式将日志级别level的日志写入到logger
//  * @details 构造一个LogEventWrap对象，包裹包含日志器和日志事件，在对象析构时调用日志器写日志事件
//  * @todo 协程id未实现，暂时写0
//  */
#define COT_LOG_FMT_LEVEL(logger, level, fmt, ...) \
    if (true)                                      \
        LogT::LogGuard(logger,                     \
                      Sptr<LogT::Event>(new LogT::Event("nihao", level, __LINE__, __FILE__, 0, UtilT::GetThreadId(), "tname_placeholder", UtilT::GetFiberId(), time(0)))   \
        ).GetLogEvent()->Printf(fmt, __VA_ARGS__)\


#define COT_LOG_FMT_FATAL(logger, fmt, ...)  COT_LOG_FMT_LEVEL(logger, LogT::Level::FATAL, fmt, __VA_ARGS__)

#define COT_LOG_FMT_ALERT(logger, fmt, ...)  COT_LOG_FMT_LEVEL(logger, LogT::Level::ALERT, fmt, __VA_ARGS__)

#define COT_LOG_FMT_CRIT(logger, fmt, ...)   COT_LOG_FMT_LEVEL(logger, LogT::Level::CRIT, fmt, __VA_ARGS__)

#define COT_LOG_FMT_ERROR(logger, fmt, ...)  COT_LOG_FMT_LEVEL(logger, LogT::Level::ERROR, fmt, __VA_ARGS__)

#define COT_LOG_FMT_WARN(logger, fmt, ...)   COT_LOG_FMT_LEVEL(logger, LogT::Level::WARN, fmt, __VA_ARGS__)

#define COT_LOG_FMT_NOTICE(logger, fmt, ...) COT_LOG_FMT_LEVEL(logger, LogT::Level::NOTICE, fmt, __VA_ARGS__)

#define COT_LOG_FMT_INFO(logger, fmt, ...)   COT_LOG_FMT_LEVEL(logger, LogT::Level::INFO, fmt, __VA_ARGS__)

#define COT_LOG_FMT_DEBUG(logger, fmt, ...)  COT_LOG_FMT_LEVEL(logger, LogT::Level::DEBUG, fmt, __VA_ARGS__)

namespace LogT {

class Logger;
class LoggerManager;
class Event;
class AppenderProxyBase;

/**
 * @brief 日志器，用于输出日志。这个类是直接与用户进行交互的类，提供 log 方法用于输出日志事件。不带root logger。
 *  Logger的实现包含了日志级别，日志器名称，创建时间，以及一个 LogAppender 数组，
 * 日志事件由 log 方法输出，log 方法首先判断日志级别是否达到本 Logger 的级别要求，
 * 是则将日志传给各个 LogAppender 进行输出，否则抛弃这条日志。
 */
class Logger : public std::enable_shared_from_this<Logger>{
public:
    using MutexType = Cot::SpinMutex;

    explicit Logger(std::string name = "noname");

    void Log(Sptr<Event> event);

    void AddAppender(Sptr<AppenderProxyBase> appender);
    void DelAppender(Sptr<AppenderProxyBase> appender);
    void ClearAppender();

    auto GetName() const
        -> std::string_view { return name_; }

    auto GetLevel() const
        -> Level { return level_; }

    auto SetLevel(Level level)
        -> void { level_ = level; }

    /**
     * @brief 获取创建时间
     */
    auto GetCreateTime() const
        -> const uint64_t& { return create_time_; }

private:
    MutexType mtx_;
    std::string name_;                             // 日志器名称
    Level level_ = Level::DEBUG;                                  // 日志级别
    std::list<Sptr<AppenderProxyBase>> appenders_; // Appender集合
    uint64_t create_time_;                         // 创建时间
};

} // namespace LogT

#endif