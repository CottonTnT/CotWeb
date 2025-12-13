#pragma once

#include "AppenderProxy.hpp"
#include "LogFormatter.h"
#include "common/alias.h"
#include <cstddef>
#include  <fstream>


class StdoutAppender {
public:
    static void log(const LogFormatter& fmter, const LogEvent& event);
};


//todo:to test, to make it better
class RollingFileAppender {

private:
    static constexpr size_t c_default_max_file_size = 64_mb ; 
    static constexpr Seconds c_default_roll_interval = Seconds{24 * 60 * 60}; // 24 hours
    // ---  Flush 策略相关常量 ---
    static constexpr Seconds c_flush_interval_seconds = Seconds{3}; // 3秒强制刷新一次
    static constexpr uint64_t c_flush_max_appends = 1024;          // 每 1024 次写入强制刷新

    std::mutex mutex_;
    //文件路径和名称
    std::string filename_;
    std::string basename_; // 用于重命名时构建新文件名
    std::ofstream filestream_;


    // 滚动机制配置
    const size_t max_bytes_; // 单个日志文件的最大字节数
    const Seconds roll_interval_; // 滚动时间间隔

    // 状态追踪
    TimePoint last_open_time_ = TimePoint::min();
    bool reopen_error_ = false;
    size_t offset_ = 0; //当前已写入的字节数

    // flush 状态追踪 ---
    TimePoint last_flush_time_ = TimePoint::min(); // 上次刷新时间
    uint64_t flush_count_ = 0; // 写入次数计数器

    auto openFile_()
        -> void;

    /**
     * @brief  **滚动日志文件**：关闭当前文件，重命名它，并打开一个新的同名文件。
     */
    auto rollFile_()
        -> void;
    auto shouldRoll_() const
        -> bool;
    auto getNewLogFileName_() const
        -> std::string;

public:
    explicit RollingFileAppender(std::string filename,
                                 size_t max_bytes_     = c_default_max_file_size,
                                 Seconds roll_interval = c_default_roll_interval);
    RollingFileAppender(const RollingFileAppender&)            = delete;
    RollingFileAppender(RollingFileAppender&&)                 = delete;
    auto operator=(const RollingFileAppender&) -> RollingFileAppender& = delete;
    auto operator=(RollingFileAppender&&) -> RollingFileAppender&      = delete;
    ~RollingFileAppender();

    void log(const LogFormatter& fmter, const LogEvent& event);

};

// todo
class SocketAppender;


//todo
class SystemAppender;

