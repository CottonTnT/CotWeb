#pragma once

#include <type_traits>
namespace LogT{

enum class Level {
    /// 错误
    ERROR = 700,
    /// 警告
    WARN = 600,
    /// 一般信息
    INFO = 500,
    /// 调试信息
    DEBUG = 400,
    /// 未设置
    ALL = 300
};

inline auto operator>=(Level one, Level two)
    -> bool
{
    return static_cast<std::underlying_type_t<Level>>(one) >=
           static_cast<std::underlying_type_t<Level>>(two);
}

} // namespace LogT


