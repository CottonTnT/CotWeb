#pragma once
#include "logger/InlineBuffer.hpp"

#include <cstddef>
#include <memory>
#include <string>

namespace X::detail {

class LogRecordView;
class FormatItem;

constexpr std::size_t MAX_FORMATTED_RECORD_INLINE_CAPACITY = 256;

using FormattedRecordBuffer = InlineBuffer<MAX_FORMATTED_RECORD_INLINE_CAPACITY>;

class LogFormatter;
using LogFormatterPtr = std::shared_ptr<const LogFormatter>;

/**
 * @details 模板参数说明:
 * - %m 消息
 * - %p 日志级别
 * - %c 日志器名称
 * - %d 日期时间，后面可跟一对括号指定时间格式，比如%%d{%%Y-%%m-%%d %%H:%%M:%%S}，这里的格式字符与 C 语言 strftime 一致
 * - %r 该日志器创建后的累计运行毫秒数
 * - %f 文件名
 * - %l 行号
 * - %t 线程id
 * - %F 协程id
 * - %N 线程名称
 * - %% 百分号
 * - %T 制表符
 * - %n 换行
 *
 * 默认格式：%%d{%%Y-%%m-%%d %%H:%%M:%%S}%%T%%t%%T%%N%%T%%F%%T[%%p]%%T[%%c]%%T%%f:%%l%%T%%m%%n
 *
 * 默认格式描述：年-月-日 时:分:秒 [累计运行毫秒数] \t 线程id \t 线程名称 \t 协程id \t [日志级别] \t [日志器名称] \t 文件名:行号 \t 日志消息 换行符
 */

class LogFormatter {
private:
    std::string pattern_;
    std::vector<std::unique_ptr<const FormatItem>> formatItemList_;

    auto parsePattern_()
        -> int;

public:
    explicit LogFormatter(std::string pattern);

    [[nodiscard]] auto format(const LogRecordView& event) const
        -> FormattedRecordBuffer;

    [[nodiscard]] auto getPattern() const noexcept
        -> std::string_view
    {
        return pattern_;
    }

}; // class FormatPattern

} // namespace X::detail
