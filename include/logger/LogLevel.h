#pragma once

#include "common/XType.h"
#include <string_view>

namespace X {

enum class LogLevel : u8 {
    /// 错误
    ERROR = 6,
    /// 警告
    WARN = 5,
    /// 一般信息
    INFO = 3,
    /// 调试信息
    DEBUG = 2,
};

auto logLevel2String(LogLevel level)
    -> std::string_view;

auto string2LogLevel(std::string_view str)
    -> LogLevel;

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
}