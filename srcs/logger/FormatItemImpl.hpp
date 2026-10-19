#include "FormatItem.h"
#include "FormatItemProxy.hpp"
#include "LogFormatter.h"
#include "LogRecordView.h"
#include "logger/LogLevel.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <string_view>
#include <sys/types.h>
#include <unordered_map>

namespace X::detail {

/* ======================== FormatterItem ======================== */

class LiteralFormatItem {
public:
    explicit LiteralFormatItem(std::string str)
        : str_ { std::move(str) }
    {
    }

    auto format(const LogRecordView& event, FormattedRecordBuffer& buf)
    {
        buf.append(str_);
    }

    // auto getSubpattern() & -> const std::string&
    // {
    //     return str_;
    // }

    // auto getSubpattern() && -> std::string
    // {
    //     return std::move(str_);
    // }

private:
    std::string str_;
};

/**
 * @brief 消息format
 */
class CustomMessageFormatItem {
public:
    auto format(const LogRecordView& event, FormattedRecordBuffer& buf)
    {
        buf.append(event.custom_msg_);
    }
};

// 日志级别format
class LogLevelFormatItem {
public:
    auto format(const LogRecordView& event, FormattedRecordBuffer& buf)
    {
        auto sv = LevelToString(event.logLevel_);
        return sv.size();
    }
};

/**
 * @brief 执行时间format
 */
class ElapseFormatItem {
public:
    auto format(const LogRecordView& event, FormattedRecordBuffer& buf)
    {
        auto count = std::chrono::duration_cast<std::chrono::milliseconds>(event.elapsed_);
        buf.appendInteger(count.count());
    }
};

/**
 * @brief 日志器名称format
 */
class LoggerNameFormatItem {
public:
    auto format(const LogRecordView& event, FormattedRecordBuffer& buf)
    {
        buf.append(event.loggerName_);
    }
};

/**
 * @brief 线程ID format
 */
class ThreadIdFormatItem {
public:
    auto format(const LogRecordView& event, FormattedRecordBuffer& buf)
    {
        buf.appendInteger(event.threadId_);
    }
};

// /**
//  * @brief 协程Id format
//  */
// class FiberIdFormatItem {
// public:
//     auto format(const LogRecordView& event, FormattedRecordBuffer& buf)

//     {
//         buf.appendInteger(event)
//     }
// };

/**
 * @brief 线程名称format
 */
class ThreadNameFormatItem {
public:
    auto format(const LogRecordView& event, FormattedRecordBuffer& buf)

    {
        buf.append(event.threadName_);
    }
};

class NewLineFormatItem {
public:
    auto format(const LogRecordView& event, FormattedRecordBuffer& buf)
    {
        buf.append("\n");
    }
};

/**
 * @brief 文件名format
 */
class FilenameFormatItem {
public:
    auto format(const LogRecordView& event, FormattedRecordBuffer& buf)
    {
        buf.append(event.filename_);
    }
};

/**
 * @brief 行号format
 */
class LineFormatItem {
public:
    auto format(const LogRecordView& event, FormattedRecordBuffer& buf)
    {
        buf.appendInteger(event.lineNumber_);
    }
};

/**
 * @brief tab format
 */

class TabFormatItem {
public:
    auto format(const LogRecordView& event, FormattedRecordBuffer& buf)
    {
        buf.append("\t");
    }
};

class PercentSignFormatItem {
public:
    auto format(const LogRecordView& event, FormattedRecordBuffer& buf)
    {
        buf.append("%");
    }
};

/* ======================== FormatItem with Status ======================== */
/**
 * @brief 时间format
 */
class DateTimeFormatItem {

private:
    static constexpr std::string_view DEFAULT_DATE_FORMAT = "%Y-%m-%d %H:%M:%S";

    std::string dateFormat_;

public:
    explicit DateTimeFormatItem(std::string date_format)
        : dateFormat_ { !date_format.empty() ? std::move(date_format) : std::string { DEFAULT_DATE_FORMAT } }
    {
    }

    // todo:exception
    auto format(const LogRecordView& event, FormattedRecordBuffer& buf)
    {
        // Warning: 同一个进程中的datetime format要一致，否则会串
        static thread_local time_t lastSec = 0;
        static thread_local char cachedDatetime[32] = { '\0' };

        const auto duration = event.timestamp_.time_since_epoch();
        const time_t curSec = static_cast<time_t>(std::chrono::duration_cast<std::chrono::seconds>(duration).count());
        if (curSec != lastSec) {
            std::tm buffer { };
#if defined(_WIN32)
            const errno_t result = localtime_s(&buffer, &current_second);
            ASSERT_RETNONE2(result == 0, "failed to convert log time to local time");
#else
            /**
             * @brief localtime_r是线程安全的，但是消耗也大：
             * 1. 全局有锁可能排队
             * 2. 需要进行复杂的时区计算(尤其是有过夏令时变更历史的时区)
             * 3. 如果系统没有加载时区信息或者要求响应时区变更, 每次调用这个接口还要去发起磁盘IO，读系统文件/etc/localtime
             */
            const std::tm* result = localtime_r(&curSec, &buffer);
            // ASSERT_RETNONE2(result != nullptr, "failed to convert log time to local time");
#endif
            const std::size_t size = std::strftime(cachedDatetime, sizeof(cachedDatetime), dateFormat_.c_str(), &buffer);
            // ASSERT_RETNONE2(size != 0, "failed to format log time");
            lastSec = curSec;
        }

        buf.append(cachedDatetime);
    }

    // auto getSubpattern() & -> const std::string&
    // {

    //     return dateFormat_;
    // }

    // auto getSubpattern() && -> std::string
    // {
    //     return std::move(dateFormat_);
    // }
};

using FormatItemFactoryFunc = std::unique_ptr<const FormatItem> (*)(std::string);

template <typename ItemImpl>
static auto createSimpleFormatItem(std::string /*unused*/)
    -> std::unique_ptr<const FormatItem>
{
    return std::make_unique<FormatItemProxy<ItemImpl>>();
}

template <typename ItemImpl>
static auto createStatusFormatItem(std::string conf)
    -> std::unique_ptr<const FormatItem>
{
    return std::make_unique<FormatItemProxy<ItemImpl>>(conf);
}

static auto getFormatItemFactoryMap()
    -> const std::unordered_map<char, FormatItemFactoryFunc>&
{
    static auto FACTORY_MAP = std::unordered_map<char, FormatItemFactoryFunc> {
        { 'm', &createSimpleFormatItem<CustomMessageFormatItem> },
        { 'p', &createSimpleFormatItem<LogLevelFormatItem> },
        { 'c', &createSimpleFormatItem<LoggerNameFormatItem> },
        { 'r', &createSimpleFormatItem<ElapseFormatItem> },
        { 'f', &createSimpleFormatItem<FilenameFormatItem> },
        { 'l', &createSimpleFormatItem<LineFormatItem> },
        { 't', &createSimpleFormatItem<ThreadIdFormatItem> },
        { 'N', &createSimpleFormatItem<ThreadNameFormatItem> },
        { 'T', &createSimpleFormatItem<TabFormatItem> },
        { 'n', &createSimpleFormatItem<NewLineFormatItem> },
        { '%', &createSimpleFormatItem<PercentSignFormatItem> },
        { 'd', &createStatusFormatItem<DateTimeFormatItem> }

    };
    return FACTORY_MAP;
}

auto createKeyFormatItem(char key, std::string conf)
    -> std::unique_ptr<const FormatItem>
{
    const auto& factories = getFormatItemFactoryMap();
    const auto it = factories.find(key);
    // ASSERT_RETVAL2(it != factories.end(), nullptr, "unknown logger format directive: " << key);
    return it->second(conf);
}

auto createLiteralFormatItem(std::string literal)
    -> std::unique_ptr<const FormatItem>
{
    return createStatusFormatItem<LiteralFormatItem>(std::move(literal));
}

} // namespace X::detail