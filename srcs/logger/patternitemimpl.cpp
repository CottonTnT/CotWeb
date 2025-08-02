#include "logger/logevent.h"
#include "logger/logutil.h"

#include "functional"
#include "logger/patternitembase.h"
#include "logger/patternitemproxy.hpp"
#include <array>

namespace LogT{

/* ======================== FormatterItem ======================== */
/**
 * @brief 消息format
 */
class MessageFormatItem {
public:
    static void Format(std::ostream& os,
                       Sptr<Event> event)
    {
        os << event->GetContent();
    }
    
};

//日志级别format
class LevelFormatItem {
public:
    static void Format(std::ostream& os,
                       Sptr<Event> event)
    {
        os << LevelToString(event->GetLevel());
    }

};

/**
 * @brief 执行时间format
 */
class ElapseFormatItem {
public:
    static void Format(std::ostream& os,
                       Sptr<Event> event)
    {
        os << event->GetElapse();
    }

};

/**
 * @brief 日志器名称format
 */
class NameFormatItem {
public:
    static void Format(std::ostream& os,
                       Sptr<Event> event)
    {
        os << event->GetLoggerName();
    }
};

/**
 * @brief 线程ID format
 */
class ThreadIdFormatItem {
public:
    static void Format(std::ostream& os,
                       Sptr<Event> event)
    {
        os << event->GetThreadId();
    }
};

/**
 * @brief 协程Id format
 */
class FiberIdFormatItem {
public:
    static void Format(std::ostream& os,
                       Sptr<Event> event)

    {
        os << event->GetFiberId();
    }
};

/**
 * @brief 线程名称format
 */
class ThreadNameFormatItem {
public:
    static void Format(std::ostream& os,
                       Sptr<Event> event)
    {
        os << event->GetThreadName();
    }
};

class NewLineFormatItem {
public:
    static void Format(std::ostream& os,
                       Sptr<Event> event)
    {
        os << "\n";
    }
};

/**
 * @brief 文件名format
 */
class FilenameFormatItem {
public:
    static void Format(std::ostream& os,
                       Sptr<Event> event)
    {
        os << event->GetFilename();
    }
};

/**
 * @brief 行号format
 */
class LineFormatItem {
public:
    static void Format(std::ostream& os,
                       Sptr<Event> event)
    {
        os << event->GetLine();
    }
};

/**
 * @brief tab format
 */

class TabFormatItem {
public:
    static void Format(std::ostream& os, Sptr<Event> event)
    {
        os << "\t";
    }
};

class PercentSignFormatItem {
public:
    static void Format(std::ostream& os, Sptr<Event> event)
    {
        os << "%";
    }
};


/* ======================== FormatItem with Status ======================== */
/**
 * @brief 时间format
 */
class DateTimeFormatItem {
public:
    DateTimeFormatItem(std::string_view date_format)
        : date_format_(date_format)
    {
    }

    //todo:exception
    void Format(std::ostream& os,
                Sptr<Event> event)
    {
        auto time = event->GetTime();

        struct tm tm;
        localtime_r(&time, &tm);

        auto  buf = std::array<char, 64>{};
         
        std::ignore = strftime(buf.data(), buf.size(), date_format_.c_str(), &tm);

        os << buf.data();
    }

    auto GetSubpattern() &
        -> const std::string&
    {

        return date_format_;
    }

    auto GetSubpattern() &&
        -> std::string
    {
        return std::move(date_format_);
    }

private:
    std::string date_format_ = "%Y-%m-%d %H:%M:%S";
};


class StringFormatItem {
public:
    StringFormatItem(std::string_view str)
        : str_(str){}

    void Format(std::ostream& os,
                Sptr<Event> event)
    {
        os << str_;
    }

    auto GetSubpattern() &
        -> const std::string&
    {
        return str_;
    }

    auto GetSubpattern() &&
        -> std::string
    {
        return std::move(str_);
    }

private:
    std::string str_;

};

using ItemProduceFunc = std::function<Sptr<PatternItemProxyBase>()>;
auto RegisterFormatItem()
    -> std::unordered_map<std::string, ItemProduceFunc>
{
    auto func_map = std::unordered_map<std::string, ItemProduceFunc> {
#define XX(str, ItemType) \
    {                      \
        #str, []() -> Sptr<PatternItemProxyBase> {\
            return Sptr<PatternItemProxyBase>{new PatternItemProxy<ItemType>{}};              \
        }\
    }\

    XX(m, MessageFormatItem),    // m:消息
    XX(p, LevelFormatItem),      // p:日志级别
    XX(c, NameFormatItem),       // c:日志器名称
    XX(r, ElapseFormatItem),     // r:累计毫秒数
    XX(f, FilenameFormatItem),   // f:文件名
    XX(l, LineFormatItem),       // l:行号
    XX(t, ThreadIdFormatItem),   // t:编程号
    XX(F, FiberIdFormatItem),    // F:协程号
    XX(N, ThreadNameFormatItem), // N:线程名称
    XX(T, TabFormatItem),        // T:制表符
    XX(n, NewLineFormatItem),    // n:换行符
    XX(%, PercentSignFormatItem),    // n:换行符
#undef XX

    };
    return func_map;
}

using StatusItemProduceFunc = std::function<Sptr<PatternItemProxyBase>(std::string)>;

auto RegisterFormatStatusItem()
    -> std::unordered_map<std::string, StatusItemProduceFunc>
{

    return std::unordered_map<std::string, StatusItemProduceFunc>
    {

#define XX(str, ItemType) \
    {                      \
        #str, [](std::string sub_pattern) \
            -> Sptr<PatternItemProxyBase> { \
                return std::static_pointer_cast<PatternItemProxyBase>(std::make_shared<PatternItemProxy<ItemType>>(sub_pattern)); \
        }\
    }\

    XX(d, DateTimeFormatItem),
    XX(str, StringFormatItem),

#undef XX

    };
}

} //namespace LogT