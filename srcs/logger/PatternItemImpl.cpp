#include "common/alias.h"
#include "logger/LogEvent.h"
#include "logger/PatternItemFacade.h"
#include "logger/PatternItemProxy.hpp"

#include <cstddef>
#include <functional>
#include <sys/types.h>
#include <string.h>

/* ======================== FormatterItem ======================== */
/**
 * @brief 消息format
 */
class MessageFormatItem {
public:
    static auto format(std::ostream& os,
                       const LogEvent& event) -> size_t
    {
        const auto& content = event.getContent();
        os << content;
        return content.size();
    }
};

// 日志级别format
class LevelFormatItem {
public:
    static auto format(std::ostream& os,
                       const LogEvent& event)
        -> size_t
    {
        auto sv = LevelToString(event.getLevel());
        os << sv;
        return sv.size();
    }
};

/**
 * @brief 执行时间format
 */
class ElapseFormatItem {
public:
    static auto format(std::ostream& os,
                       const LogEvent& event)
        -> size_t
    {
        os << event.getElapse();
        return sizeof(decltype(event.getElapse()));
    }
};

/**
 * @brief 日志器名称format
 */
class NameFormatItem {
public:
    static auto format(std::ostream& os,
                       const LogEvent& event)
        -> size_t
    {
        const auto& name = event.getLoggerName();
        os << event.getLoggerName();
        return name.size();
    }
};

/**
 * @brief 线程ID format
 */
class ThreadIdFormatItem {
public:
    static auto format(std::ostream& os,
                       const LogEvent& event)
        -> size_t
    {
        os << event.getThreadId();
        return sizeof(decltype(event.getThreadId()));
    }
};

/**
 * @brief 协程Id format
 */
class FiberIdFormatItem {
public:
    static auto format(std::ostream& os,
                       const LogEvent& event)
        -> size_t

    {
        os << event.getFiberId();
        return sizeof(decltype(event.getThreadId()));
    }
};

/**
 * @brief 线程名称format
 */
class ThreadNameFormatItem {
public:
    static auto format(std::ostream& os,
                       const LogEvent& event)
        -> size_t
    {
        const auto& name = event.getThreadName();
        os << event.getThreadName();
        return name.size();
    }
};

class NewLineFormatItem {
public:
    static auto format(std::ostream& os,
                       const LogEvent& /*event*/)
        -> size_t
    {
        os.put('\n');
        return 1;
    }
};

/**
 * @brief 文件名format
 */
class FilenameFormatItem {
public:
    static auto format(std::ostream& os,
                       const LogEvent& event)
        -> size_t
    {
        const auto& filename = event.getFilename();
        os << event.getFilename();
        return filename.size();
    }
};

/**
 * @brief 行号format
 */
class LineFormatItem {
public:
    static auto format(std::ostream& os,
                       const LogEvent& event)
        -> size_t
    {
        os << event.getLine();
        return sizeof(decltype(event.getLine()));
    }
};

/**
 * @brief tab format
 */

class TabFormatItem {
public:
    static auto format(std::ostream& os, const LogEvent& /*event*/)
        -> ssize_t
    {
        os.put('\t');
        return 1;
    }
};

class PercentSignFormatItem {
public:
    static auto format(std::ostream& os, const LogEvent& /*event*/)
        -> size_t
    {
        os << "%";
        return 1;
    }
};

/* ======================== FormatItem with Status ======================== */
/**
 * @brief 时间format
 */
class DateTimeFormatItem {
public:
    explicit DateTimeFormatItem(std::string date_format)
        : date_format_ {std::move(date_format)}
    {
    }

    // todo:exception
    auto format(std::ostream& os,
                const LogEvent& event)
        -> size_t
    {
        auto time = event.getTime();

        struct tm tm;
        localtime_r(&time, &tm);

        char buf[64] = {'\0'};
        std::ignore  = strftime(buf, 63, date_format_.c_str(), &tm);

        os << buf;
        return strlen(buf);
    }

    auto getSubpattern() & -> const std::string&
    {

        return date_format_;
    }

    auto getSubpattern() && -> std::string
    {
        return std::move(date_format_);
    }

private:
    std::string date_format_ = "%Y-%m-%d %H:%M:%S";
};

class StringFormatItem {
public:
    explicit StringFormatItem(std::string str)
        : str_ {std::move(str)}
    {
    }

    auto format(std::ostream& os,
                const LogEvent& /*event*/)
        -> size_t
    {
        os << str_;
        return str_.size();
    }

    auto getSubpattern() & -> const std::string&
    {
        return str_;
    }

    auto getSubpattern() && -> std::string
    {
        return std::move(str_);
    }

private:
    std::string str_;
};

using ItemFactoryFunc = std::function<Sptr<PatternItemFacade>()>;
auto RegisterItemFactoryFunc()
    -> std::unordered_map<std::string, ItemFactoryFunc> {
        auto func_map = std::unordered_map<std::string, ItemFactoryFunc> {
#define XX(str, ItemType)                                                       \
    {                                                                           \
        #str, []() -> Sptr<PatternItemFacade> {                                 \
            return Sptr<PatternItemFacade> {new PatternItemProxy<ItemType> {}}; \
        }                                                                       \
    }

            XX(m, MessageFormatItem),     // m:消息
            XX(p, LevelFormatItem),       // p:日志级别
            XX(c, NameFormatItem),        // c:日志器名称
            XX(r, ElapseFormatItem),      // r:累计毫秒数
            XX(f, FilenameFormatItem),    // f:文件名
            XX(l, LineFormatItem),        // l:行号
            XX(t, ThreadIdFormatItem),    // t:编程号
            XX(F, FiberIdFormatItem),     // F:协程号
            XX(N, ThreadNameFormatItem),  // N:线程名称
            XX(T, TabFormatItem),         // T:制表符
            XX(n, NewLineFormatItem),     // n:换行符
            XX(%, PercentSignFormatItem), // n:换行符
#undef XX

        };
return func_map;
}

using StatusItemFactoryFunc = std::function<Sptr<PatternItemFacade>(std::string)>;

auto RegisterStatusItemFactoryFunc()
    -> std::unordered_map<std::string, StatusItemFactoryFunc> {

        return std::unordered_map<std::string, StatusItemFactoryFunc> {

#define XX(str, ItemType)                                                                                                  \
    {                                                                                                                      \
        #str, [](std::string sub_pattern) -> Sptr<PatternItemFacade> {                                                     \
            return std::static_pointer_cast<PatternItemFacade>(std::make_shared<PatternItemProxy<ItemType>>(sub_pattern)); \
        }                                                                                                                  \
    }

            XX(d, DateTimeFormatItem),
            XX(str, StringFormatItem),

#undef XX

        };
}
