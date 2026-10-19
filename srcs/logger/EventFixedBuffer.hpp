#pragma once
#include "LogRecordView.h"
#include <array>
// 假设 LogEvent 包含了所有必要的日志信息
// class LogEvent { /* ... */ };

// 定义缓冲区大小：存储 64 个 LogEvent

namespace X::detail {
constexpr size_t DEFAULT_EVENT_MAX_SIZE = 64;

template <size_t N = DEFAULT_EVENT_MAX_SIZE>
class EventFixedBuffer {
public:
    using EventArray = std::array<X::detail::LogRecordView, N>;
    EventFixedBuffer()
        : count_(0)
    {
    }
    // 尝试将 LogEvent 写入缓冲区
    auto append(LogRecordView event)
        -> bool
    {
        if (count_ < DEFAULT_EVENT_MAX_SIZE) {
            data_[count_] = std::move(event); // 复制 LogEvent 对象
            count_++;
            return true;
        }
        return false; // 缓冲区已满
    }

    // 已写入的事件数量
    [[nodiscard]] auto count() const -> size_t { return count_; }
    // 剩余可用空间（事件数）
    [[nodiscard]] auto available() const -> size_t { return N - count_; }
    // 获取事件数组的起始指针
    [[nodiscard]] auto data() const
        -> const LogRecordView* { return data_.data(); }
    auto getEventSpan()
        -> std::span<LogRecordView>
    {
        return std::span<LogRecordView>(data_.data(), count_);
    }
    // 缓冲区清零（重置事件计数）
    void reset() { count_ = 0; }

private:
    EventArray data_;
    size_t count_; // 当前存储的事件数量，代替了原来的字节偏移
};
} // namespace X::detail