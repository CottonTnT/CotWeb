#pragma once

#include <type_traits>
namespace LogT{

enum class Level {
    /// 错误
    ERROR = 300,
    /// 警告
    WARN = 400,
    /// 一般信息
    INFO = 600,
    /// 调试信息
    DEBUG = 700,
    /// 未设置
    ALL = 800
};

inline auto operator>=(Level one, Level two)
    -> bool
{
    return static_cast<std::underlying_type_t<Level>>(one) >=
           static_cast<std::underlying_type_t<Level>>(two);
}

} // namespace LogT


