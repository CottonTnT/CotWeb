#pragma once
#include "common.h"
#include "patternitembase.h"

#include <list>
#include <print>

namespace LogT {

class Event;

/**
 * @details 模板参数说明:
 * - %%m 消息
 * - %%p 日志级别
 * - %%c 日志器名称
 * - %%d 日期时间，后面可跟一对括号指定时间格式，比如%%d{%%Y-%%m-%%d %%H:%%M:%%S}，这里的格式字符与 C 语言 strftime 一致
 * - %%r 该日志器创建后的累计运行毫秒数
 * - %%f 文件名
 * - %%l 行号
 * - %%t 线程id
 * - %%F 协程id
 * - %%N 线程名称
 * - %%% 百分号
 * - %%T 制表符
 * - %%n 换行
 *
 * 默认格式：%%d{%%Y-%%m-%%d %%H:%%M:%%S}%%T%%t%%T%%N%%T%%F%%T[%%p]%%T[%%c]%%T%%f:%%l%%T%%m%%n
 *
 * 默认格式描述：年-月-日 时:分:秒 [累计运行毫秒数] \t 线程id \t 线程名称 \t 协程id \t [日志级别] \t [日志器名称] \t 文件名:行号 \t 日志消息 换行符
 */
class FormatPattern {
public:
    explicit FormatPattern(std::string pattern = "%d{%Y-%m-%d %H:%M:%S} [%rms]%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n")
        : pattern_(std::move(pattern))
        , error_(false)

    {
        StartParse_();
    }
    auto Format(Sptr<Event> event)
        -> std::string;

    auto Format(std::ostream& os, Sptr<Event> event)
        -> void;
    auto Show()
        -> void;
private:
    void StartParse_();
    std::string pattern_;
    std::list<Sptr<PatternItemProxyBase>> pattern_items_;
    bool error_;
}; // class FormatPattern

} // namespace LogT