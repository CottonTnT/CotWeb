#pragma once

#include <string>
#include "common/alias.h"

/**
 * @brief Time stamp in UTC, in microseconds resolution.
 * @attention This class is immutable. It's recommended to pass it by value, since it's passed in register on x64.
 */
class Timestamp
{
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch);

    static auto Now()
        -> Timestamp;

    /**
     * @brief format the micro_seconds_since_epoch_ to string like "seconds.microseconds"
     */
    [[nodiscard]] auto toString() const
        -> std::string;
    [[nodiscard]] auto toTimePoint() const
        -> TimePoint;

    /**
     * @brief format the micro_seconds_since_epoch_ to string like "YYYYMMDD HH:MM:SS.microseconds" 
     * @param showMicroseconds whether output microseconds part
     */
    [[nodiscard]] auto ToFormattedString(bool showMicroseconds = true) const
            -> std::string;

    [[nodiscard]] auto valid() const
        -> bool  { return micro_seconds_since_epoch_ > 0; }

  // for internal usage.
    [[nodiscard]] auto microSecondsSinceEpoch() const
        -> int64_t { return micro_seconds_since_epoch_; }

    [[nodiscard]] auto SecondsSinceEpoch() const
        -> time_t;

    ///
    /// Get time of now.
    ///
    static auto Invalid()
        -> Timestamp
    {
        return Timestamp{};
    }

    static auto FromUnixTime(time_t t)
        -> Timestamp
    {
      return FromUnixTime(t, 0);
    }

    static auto FromUnixTime(time_t t, int microseconds)
        -> Timestamp;
    inline static constexpr auto c_micro_seconds_per_second = 1000 * 1000;

private:
    int64_t micro_seconds_since_epoch_;
};

inline auto operator<(Timestamp lhs, Timestamp rhs)
    -> bool
{
  return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

inline auto operator==(Timestamp lhs, Timestamp rhs)
    -> bool
{
  return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

///
/// Gets time difference of two timestamps, result in seconds.
///
/// @param high, low
/// @return (high-low) in seconds
/// @c double has 52-bit precision, enough for one-microsecond
/// resolution for next 100 years.
inline auto TimeDifference(Timestamp high, Timestamp low)
    -> double
{
  auto diff = high.microSecondsSinceEpoch() - low.microSecondsSinceEpoch();
  return static_cast<double>(diff) / Timestamp::c_micro_seconds_per_second;
}

/**
 * @brief
 *  @return timestamp+seconds as Timestamprief 
 */
inline auto addTime(Timestamp timestamp, double seconds)
    -> Timestamp
{
  auto delta = static_cast<int64_t>(seconds * Timestamp::c_micro_seconds_per_second);
  return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}