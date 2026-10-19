#pragma once

#include <memory>

namespace X {

class LogRecordView;

class LogAppender;
using AppenderPtr = std::shared_ptr<LogAppender>;

class LogAppender {
protected:
    LogAppender() = default;

public:
    LogAppender(const LogAppender&) = delete;
    auto operator=(const LogAppender&) -> LogAppender& = delete;
    LogAppender(LogAppender&&) = delete;
    auto operator=(LogAppender&&) -> LogAppender& = delete;
    virtual ~LogAppender() = default;

    virtual void append(std::string_view message) = 0;

    virtual void flush() = 0;
    virtual void sync() = 0;

    [[nodiscard]] static auto makeStdoutAppender()
        -> AppenderPtr;
    [[nodiscard]] static auto makeFileAppender(std::string file_name)
        -> AppenderPtr;
};

}
