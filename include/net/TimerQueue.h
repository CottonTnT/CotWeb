#pragma once
#include <atomic>
#include <set>
#include <vector>
#include <map>

#include "Timestamp.h"
#include "Callbacks.h"
#include "Channel.h"
#include "Timer.h"

class EventLoop;

/**
 * @brief  A best efforts timer queue. No guarantee that the callback will be on time.
 * @details It's thread safe cause it`s member function only called by the onwer eventloop, i.e called by onwer thread.
 */
class TimerQueue {
private:
    using TimerDeleter = void(*)(Timer*);

    static void timerFree_ (Timer* timer){
            delete timer;
    };
    static void doNonthingDeleter_ (Timer* timer){
            // do nothing
    };
public:
    // FIXME: use unique_ptr<Timer> instead of raw pointers.
    // This requires heterogeneous comparison lookup (N3465) from C++14
    // so that we can find an T* in a set<unique_ptr<T>>.
    // it can`t use unique_ptr<Timer> cause you can`t copy or move it from std::set

    // using Entry     = std::pair<Timestamp, Timer*>;
    using Entry = std::pair<Timestamp, std::unique_ptr<Timer, TimerDeleter>>;
    using TimerList = std::set<Entry>;

    // ActiveTimer 用于在取消时快速查找 Timer，通过 Timer* 和其序列号唯一标识
    // using ActiveTimer    = std::pair<Timer*, uint64_t>;
    using ActiveTimerMap = std::map <Timer::Id, Timer*>;
private:

    EventLoop* owner_loop_; // the owner loop

    const int timerfd_;
    Channel timerfd_channel_;
    // Timer list sorted by expiration
    TimerList timers_;

    // for cancel()
    ActiveTimerMap active_timers_;            // 存储所有活跃的 Timer，用于高效取消
    std::atomic<bool> calling_expired_timers_; /* atomic */
    std::set<Timer::Id> canceling_timers_;         // 存储在调用已到期定时器回调期间，请求取消的定时器
    void addTimerInOwnerLoop_(std::unique_ptr<Timer, TimerDeleter> timer);
    void cancelInOwnerLoop_(Timer::Id timerId);

    // called when timerfd alarms
    void timerChannelReadCB_();
    // move out all expired timers
    auto getExpired_(Timestamp now) -> std::vector<Entry>;
    void reset_(std::vector<Entry>& expired, Timestamp now);

    /**
     * @brief 负责将 Timer 对象同时插入到 timers_ (按时间排序) 和 activeTimers_ (用于取消) 两个 std::set 中，并返回新插入的定时器是否改变了“最早到期时间”
     */
    auto insert_(std::unique_ptr<Timer, TimerDeleter> timer)
        -> std::pair<bool, Timestamp>;

public:
    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();
    TimerQueue(const TimerQueue&)                    = delete;
    TimerQueue(TimerQueue&&)                         = delete;
    auto operator=(const TimerQueue&) -> TimerQueue& = delete;
    auto operator=(TimerQueue&&) -> TimerQueue&      = delete;

    /**
     * @brief  Schedules the callback to be run at given time, Must be thread safe. Usually be called from other threads.
     * @param repeats if @c interval > 0.0
     * 
     */
    auto addTimer(TimerCallback cb,
                  Timestamp when,
                  double interval)
        -> Timer::Id;

    void cancel(Timer::Id timerId);
};