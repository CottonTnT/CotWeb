
#ifndef LOGAPPENDER_H
#define LOGAPPENDER_H

#include "common.h"
#include "appenderbase.h"
#include "logformatpattern.h"

#include <fstream>

namespace LogT {

class FormatPattern;
class Event;


/**
 * @brief thread-safe, actually a proxy of appender impl
 */
template <typename Impl>
requires IsAppenderImpl<Impl>
class Appender :  public AppenderProxyBase {
public:
    Appender() = default;
    Appender(const Appender&)            = delete;
    Appender(Appender&&)                 = delete;
    Appender& operator=(const Appender&) = delete;
    Appender& operator=(Appender&&)      = delete;

    template <typename... Ts>
    explicit Appender(Sptr<FormatPattern> formatter, Ts... ts)
        : AppenderProxyBase(formatter)
        , impl_(std::forward<Ts>(ts)...)
    {
    }

    template <typename... Ts>
    explicit Appender(Ts... ts):
     impl_(std::forward<Ts>(ts)...)
    {
    }

    void Log(Sptr<Event> event) override
    {
        auto lk_guard = std::lock_guard<MutexType>(mtx_);
        impl_.Log(formatter_, event);
    }

     ~Appender() override = default;

private:
    Impl impl_;
};

class StdoutAppender {
public:
    static void Log(Sptr<FormatPattern> fmter, Sptr<Event> event);
};

//todo:to test
class FileAppender {
public:

    explicit FileAppender(std::string filename);

    void Log(Sptr<FormatPattern> fmter, Sptr<Event> event);

    auto Reopen()
        -> bool;

private:
    //文件路径
    std::string filename_;
    std::ofstream filestream_;
    //上次打开的时间
    uint64_t lasttime_ = 0;
    // 文件打开错误标识
    bool reopen_error_ = false;
};
#endif
} //namespace LogT