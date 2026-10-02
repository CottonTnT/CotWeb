#pragma once
#include <chrono>
#include <cstddef>
#include <memory>

#define INVALID64 (~0ULL)
#define INVALID32 0xFFFFFFFF
#define INVALID16 0xFFFF
#define INVALID8 0xFF

#define MAX_U8 0xFF
#define MAX_U16 0xFFFF
#define MAX_U32 0xFFFFFFFF
#define MAX_U64 (~0ULL)

using u8 = std::uint8_t;
using s8 = std::int8_t;
using u16 = std::uint16_t;
using s16 = std::int16_t;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u64 = std::uint64_t;
using s64 = std::int64_t;

/* ======================== 标准库别名 ======================== */
template <typename T>
using Sptr = std::shared_ptr<T>;

template <typename T, typename Deleter = std::default_delete<T>>
using Uptr = std::unique_ptr<T, Deleter>;

template <typename T>
using Wptr = std::weak_ptr<T>;

template <typename T, typename Alloctor = std::allocator<T>>
using List = std::vector<T, Alloctor>;

/* ======================== 时间相关别名 ======================== */
using Clock = std::chrono::steady_clock;
template <typename Rep, typename Period>
using TimeDuration = std::chrono::duration<Rep, Period>;

using TimePoint = Clock::time_point;

using Seconds = std::chrono::seconds;

/* ======================== 内存大小相关字面量 ======================== */
// using namespace std::chrono_literals;
// 基础类型：一个不可变的封装类，包含 uint64_t 值
// 尽管字面量操作符可以直接返回 uint64_t，但使用一个封装类可以提供更好的类型安全性
// 并且未来可以扩展操作符重载等功能。
class ImmutableMemorySize {
private:
    const std::size_t value_;

public:
    // 构造函数设为 constexpr, 使得对象可以在编译期构造
    constexpr explicit ImmutableMemorySize(std::size_t value) noexcept
        : value_(value)
    {
    }

    /**
     *  @brief 允许 ImmutableMemorySize 对象隐式转换为 std::size_t
     */
    constexpr operator std::size_t() const
    {
        return value_;
    }
};

// 1. 字节 (Byte) 字面量: _b
// 输入为 unsigned long long
constexpr auto operator""_b(unsigned long long bytes) noexcept -> ImmutableMemorySize
{
    // 直接返回 bytes
    return ImmutableMemorySize(static_cast<size_t>(bytes));
}

// 2. 千字节 (Kilobyte) 字面量: _kb (1 KB = 1024 B)
constexpr auto operator""_kb(unsigned long long k_bytes) noexcept -> ImmutableMemorySize
{
    // 1024 是 uint64_t 类型，确保乘法安全
    return ImmutableMemorySize(static_cast<size_t>(k_bytes) * 1024ULL);
}

// 3. 兆字节 (Megabyte) 字面量: _mb (1 MB = 1024 * 1024 B)
constexpr auto operator""_mb(unsigned long long m_bytes) noexcept -> ImmutableMemorySize
{
    // 1024 * 1024 = 1048576，确保乘法使用 ULL
    return ImmutableMemorySize(static_cast<size_t>(m_bytes) * 1024ULL * 1024ULL);
}

// 4. 吉字节 (Gigabyte) 字面量: _gb (作为扩展，1 GB = 1024 * 1024 * 1024 B)
constexpr auto operator""_gb(unsigned long long g_bytes) noexcept -> ImmutableMemorySize
{
    // 确保计算结果在 uint64_t 范围内，并使用 ULL
    return ImmutableMemorySize(static_cast<size_t>(g_bytes) * 1024ULL * 1024ULL * 1024ULL);
}
