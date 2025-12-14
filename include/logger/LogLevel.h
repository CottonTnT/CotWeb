#pragma once

#include <string_view>

enum class LogLevel {
    /// 系统错误
    SYSFATAL = 9,
    /// 系统错误
    SYSERR = 8,
    /// 致命错误
    FATAL = 7,
    /// 错误
    ERROR = 6,
    /// 警告
    WARN = 5,
    /// 追踪
    TRACE = 4,
    /// 一般信息
    INFO = 3,
    /// 调试信息
    DEBUG = 2,
    /// 未设置
    ALL = 1,
};

auto LevelToString(LogLevel level)
        ->  std::string_view;

auto StringToLogLevel(std::string_view str)
        ->  LogLevel;

auto operator<=(LogLevel lhs, LogLevel rhs)
    -> bool;
auto operator>(LogLevel lhs, LogLevel rhs)
    -> bool;
auto operator==(LogLevel one, LogLevel two)
    -> bool;

auto operator>=(LogLevel lhs, LogLevel rhs)
    -> bool;
auto operator<(LogLevel lhs, LogLevel rhs)
    -> bool;