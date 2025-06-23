#pragma once

namespace LogT{

enum class Level {
        /// 致命情况，系统不可用
        FATAL = 0,
        /// 高优先级情况，例如数据库系统崩溃
        ALERT = 100,
        /// 严重错误，例如硬盘错误
        CRIT = 200,
        /// 错误
        ERROR = 300,
        /// 警告
        WARN = 400,
        /// 正常但值得注意
        NOTICE = 500,
        /// 一般信息
        INFO = 600,
        /// 调试信息
        DEBUG = 700,
        /// 未设置
        NOTSET = 800
};

} // namespace LogT


