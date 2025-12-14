#include <ctime>

#include "net/Timestamp.h"
#include "common/alias.h"
#include "sys/time.h"
#include <array>
#include <format>
#include <sys/select.h>
#include <stdio.h>


Timestamp::Timestamp() : micro_seconds_since_epoch_(0)
{
}

Timestamp::Timestamp(uint64_t microSecondsSinceEpoch)
    : micro_seconds_since_epoch_(microSecondsSinceEpoch)
{
}

auto Timestamp::Now()
    -> Timestamp
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    auto seconds = tv.tv_sec;
    return Timestamp{ seconds * c_micro_seconds_per_second + tv.tv_usec };
}

auto Timestamp::toString() const
    -> std::string
{
  auto buf = std::array<char, 32>{} ;
  int64_t seconds = micro_seconds_since_epoch_ / c_micro_seconds_per_second;
  int64_t microseconds = micro_seconds_since_epoch_ % c_micro_seconds_per_second;
  auto stop_pos = std::format_to_n(buf.data(), 32, "{}.{:06}", seconds, microseconds);
  stop_pos.out[0] = '\0';
  return buf.data();
}

auto Timestamp::toTimePoint() const
    -> TimePoint
{
    return TimePoint{std::chrono::microseconds{micro_seconds_since_epoch_}};
}

auto Timestamp::toFormattedString(bool showMicroseconds) const
    -> std::string
{
    auto buf = std::array<char, 128>{ };
    auto tm_time = tm{};
    auto seconds_since_epoch_ = secondsSinceEpoch();
    localtime_r(&seconds_since_epoch_, &tm_time);
    auto stop_pos = std::format_to_n_result<char*>{};
    if (showMicroseconds)
    {
        stop_pos = std::format_to_n(buf.data(), 128, "{:04d}{:02d}{:02d} {:02d}:{:02d}:{:02d}.{:06d}",
                         tm_time.tm_year + 1900,
                         tm_time.tm_mon + 1,
                         tm_time.tm_mday,
                         tm_time.tm_hour,
                         tm_time.tm_min,
                         tm_time.tm_sec,
                         seconds_since_epoch_);
    }
    else
    {
        stop_pos = std::format_to_n(buf.data(), 128, "{:04d}{:02d}{:02d} {:02d}:{:02d}:{:02d}",
                         tm_time.tm_year + 1900,
                         tm_time.tm_mon + 1,
                         tm_time.tm_mday,
                         tm_time.tm_hour,
                         tm_time.tm_min,
                         tm_time.tm_sec);
    }
    stop_pos.out[0] = '\0';
    return buf.data();
}
auto Timestamp::secondsSinceEpoch() const
    -> time_t
{
    return static_cast<time_t>(micro_seconds_since_epoch_ / c_micro_seconds_per_second);
}
auto Timestamp::FromUnixTime(time_t t, int microseconds)
    -> Timestamp
{
    return Timestamp{ static_cast<int64_t>(t) * c_micro_seconds_per_second + microseconds };
}