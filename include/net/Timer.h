#pragma once
#include "Timestamp.h"
#include "Callbacks.h"
#include <atomic>

/**
 * @brief Internal class for timer event
 */
class Timer {
public:
    using Id = uint64_t;
public:
    Timer(TimerCallback cb, Timestamp when, double interval)
        : callback_ {std::move(cb)}
        , expiration_ {when}
        , interval_ {interval}
        , repeat_ {interval > 0.0}
        , id_ {s_auto_incr_id_.fetch_add(1)}
    {
    }
    Timer (const Timer&) = delete;
    auto operator=(const Timer&) -> Timer& = delete;
    Timer (Timer&&) = delete;
    auto operator=(Timer&&) -> Timer& = delete;
    ~Timer() = default;

    void run() const
    {
        callback_();
    }

    [[nodiscard]] auto getExpiration() const
        -> Timestamp { return expiration_; }
    [[nodiscard]] auto isRepeatable() const
        -> bool { return repeat_; }
    [[nodiscard]] auto getTimerId() const
        -> uint64_t { return id_; }

    void restart(Timestamp now);

    static auto getNumberOfCreatedTimer() -> uint64_t { return s_auto_incr_id_.load(std::memory_order_relaxed); }

private:
    const TimerCallback callback_;
    Timestamp expiration_;
    const double interval_;
    const bool repeat_;
    const Id id_;

    static std::atomic<Id> s_auto_incr_id_;
};
