#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstring>
#include <string_view>
#include <vector>

namespace X::detail {

// 自定义一个日志缓冲区，针对高频、小数据量的日志场景，在栈上分配日志缓冲区，遇超长日志，再在堆上分配内存(SBO)
template <std::size_t INLINE_CAPACITY>
class InlineBuffer {

public:
    void append(const char* data, std::size_t size)
    {
        if (size == 0) {
            return;
        }

        if (overflow_.empty() && size_ + size <= INLINE_CAPACITY) {
            std::memcpy(smallBuffer_.data() + size_, data, size);
            size_ += size;
            return;
        }

        if (overflow_.empty()) {
            const std::size_t required_capacity = size_ + size;
            overflow_.reserve(std::max(INLINE_CAPACITY * 2, required_capacity));
            overflow_.insert(overflow_.end(), smallBuffer_.data(), smallBuffer_.data() + size_);
        }
        overflow_.insert(overflow_.end(), data, data + size);
        size_ = overflow_.size();
    }

    void append(std::string_view value)
    {
        append(value.data(), value.size());
    }

    void append(char value)
    {
        append(&value, 1);
    }

    template <std::integral T>
    void appendInteger(T value)
    {
        std::array<char, 32> buffer { };
        // 使用std::to_string()可能会导致内存分配失败, 跑错
        const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value); // 基本上不会失败，转失败了也没关系

        // ASSERT_RETNONE2(result.ec == std::errc(), "trans " << value << " to chars failed.");
        append(buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data()));
    }

    [[nodiscard]] auto data() const noexcept
        -> const char*
    {
        return overflow_.empty() ? smallBuffer_.data() : overflow_.data();
    }

    [[nodiscard]] auto size() const noexcept
        -> std::size_t
    {
        return size_;
    }

    [[nodiscard]] auto view() const noexcept
        -> std::string_view
    {
        return { data(), size() };
    }

private:
    std::array<char, INLINE_CAPACITY> smallBuffer_ { };
    std::vector<char> overflow_;
    std::size_t size_ = 0;
};

} // namespace X::detail
