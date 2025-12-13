#include "net/TimerQueue.h"
#include "net/EventLoop.h"
#include "net/Timer.h"
#include "net/Timestamp.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <memory>
#include <sys/timerfd.h>
#include <unistd.h>
#include <utility>
#include <print>

auto createTimerfd()
    -> int
{
    // CLOCK_MONOTONIC：表示使用一个不受系统时间调整影响的时钟（适合定时器）。
    // TFD_NONBLOCK：让这个 fd 是非阻塞的。
    // TFD_CLOEXEC：在 exec 时自动关闭该 fd，避免子进程继承。
    auto timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                    TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0)
    {
        // LOG_SYSFATAL << "Failed in timerfd_create";
    }
    return timerfd;
}

/**
 * @brief 计算从现在到指定时间戳之间的相对时间间隔
 */
auto howMuchTimeFromNow(Timestamp when)
    -> struct timespec
{
    auto microseconds = when.microSecondsSinceEpoch()
                        - Timestamp::Now().microSecondsSinceEpoch();
    // 最小超时设为 100 微秒，避免过于频繁或立即触发
    microseconds = microseconds < 100 ? 100 : microseconds;
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(
        microseconds / Timestamp::c_micro_seconds_per_second);
    ts.tv_nsec = static_cast<long>(
        (microseconds % Timestamp::c_micro_seconds_per_second) * 1000);
    return ts;
}

/**
 * @brief 读取 timerfd 文件描述符，清除定时器到期事件
 */
void
readTimerfd(int timerfd, [[maybe_unused]] Timestamp now)
{
    uint64_t howmany = 0;
    // 根据 Linux timerfd 的规范，当定时器到期时，内核会在 timerfd 上写入一个 8 字节的 uint64_t 值。这个值表示自上次成功读取以来，定时器到期了多少次
    auto n = ::read(timerfd, &howmany, sizeof howmany);
    //   LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
    if (n != sizeof howmany)
    {
        // LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
    }
}
/**
 * @brief 设置或重置 timerfd 的下一次到期时间
 */
void resetTimerfd(int timerfd, Timestamp expiration)
{
    // wake up loop by timerfd_settime()
    auto new_value = itimerspec {};
    auto old_value = itimerspec {};

    // 计算相对超时时间
    new_value.it_value = howMuchTimeFromNow(expiration);

    // flag = 0 new_value 中的时间是相对时间，不关心 oldValue
    auto ret = ::timerfd_settime(timerfd, 0, &new_value, &old_value);
    if (ret)
    {
        // LOG_SYSERR << "timerfd_settime()";
    }
}

TimerQueue::TimerQueue(EventLoop* loop)
    : owner_loop_ {loop}
    , timerfd_ {createTimerfd()}
    , timerfd_channel_ {loop, timerfd_}
    , calling_expired_timers_ {false}
{

    timerfd_channel_.setReadCallback([timerqueue = this](Timestamp) { timerqueue->timerChannelReadCB_(); });
    // we are always reading the timerfd, we disarm it with timerfd_settime.
    // 当所有的定时器都被移除后，TimerQueue 会解除武装（Disarm）timerfd，方法是将下一次到期时间设置为一个不会触发的值（例如，it_value 设为 0，这会导致 timerfd 立即被清除可读状态，但不会再次触发），或者不再调用 resetTimerfd
    timerfd_channel_.enableReading();
}

TimerQueue::~TimerQueue()
{
    timerfd_channel_.unregisterAllEvent();
    timerfd_channel_.remove();
    ::close(timerfd_);
    // do not remove channel, since we're in EventLoop::dtor();
    // for (const Entry& timer : timers_)
    // {
    //     delete timer.second;
    // }
}

auto TimerQueue::addTimer(TimerCallback cb,
                          Timestamp when,
                          double interval)
    -> Timer::Id
{
    auto new_timer = std::make_unique<Timer>(std::move(cb), when, interval);
    auto id        = new_timer->getTimerId();
    owner_loop_->runTask([this, new_timer = new_timer.release()]() mutable {
        this->addTimerInOwnerLoop_(std::unique_ptr<Timer, TimerDeleter>{new_timer, timerFree_});
    });
    //返回新定时器的唯一标识符
    return id;
}

void TimerQueue::cancel(Timer::Id timerId)
{
    owner_loop_->runTask([timerqueue = this, timerId]() {
        timerqueue->cancelInOwnerLoop_(timerId);
    });
}

void TimerQueue::addTimerInOwnerLoop_(std::unique_ptr<Timer, TimerDeleter> timer)
{
    owner_loop_->assertInOwnerThread();
    auto [is_earliest, next_expire_time] = insert_(std::move(timer));
    if (is_earliest) // 如果新插入的定时器成为了最早到期的
    {
        // 重置 timerfd 的超时时间为这个新定时器的到期时间
        resetTimerfd(timerfd_,  next_expire_time);
    }
}

void TimerQueue::cancelInOwnerLoop_(Timer::Id timerId)
{
    owner_loop_->assertInOwnerThread();
    assert(timers_.size() == active_timers_.size());

    // 使用 TimerId 中的 Timer* 和 sequence_ 构造 ActiveTimer 用于查找

    // 如果在 activeTimers_ 中找到了该定时器
    if (active_timers_.contains(timerId))
    {
        auto* timer = active_timers_[timerId];
        // auto timer = *it->first;
        // 注意：timers_ 的 key 是 pair<Timestamp, Timer*>，需要用其到期时间和指针来构造
        // 从 timers_ 中移除对应的 Entry
        auto n = timers_.erase(Entry {timer->getExpiration(), std::unique_ptr<Timer, TimerDeleter>{timer, doNonthingDeleter_}});
        assert(n == 1);
        (void)n;
        // delete timer; // FIXME: no delete please
        // 从 activeTimers_ 中移除
        active_timers_.erase(timerId);
    }
    else if (calling_expired_timers_) // 如果没找到，且当前正在执行回调
    {
        // 这意味着要取消的定时器可能是一个刚刚到期并在被处理的重复定时器。
        // 将其加入 cancelingTimers_ 集合。
        // reset() 方法在重新插入重复定时器前会检查这个 cancelingTimers_ 集合，
        // 如果在其中，则不会重新插入，从而达到取消的目的。
        canceling_timers_.insert(timerId);
    }
    assert(timers_.size() == active_timers_.size());
}

void TimerQueue::timerChannelReadCB_()
{
    owner_loop_->assertInOwnerThread();

    auto now = Timestamp::Now();

    // 1. 读取 timerfd，清空事件，避免重复触发
    readTimerfd(timerfd_, now);

    // 2. 获取所有在 'now' 时刻之前或同时到期的定时器
    auto expired = getExpired_(now);

    calling_expired_timers_ = true; // 标记正在调用回调
    canceling_timers_.clear();      // 清空上次调用期间的取消列表

    // safe to callback outside critical section
    std::ranges::for_each(expired, [](const auto& entry) {
        entry.second->run();
    });
    calling_expired_timers_ = false; // 标记回调调用结束

    // 4. 重置到期的定时器中设置为重复的定时器，并设置 timerfd 的下一次超时时间
    reset_(expired, now);
}

auto TimerQueue::getExpired_(Timestamp now)
    -> std::vector<Entry>
{
    assert(timers_.size() == active_timers_.size());
    auto expired_entrys = std::vector<Entry> {};

    // 构造一个哨兵 Entry，其时间戳为 now，Timer* 为一个不可能的地址 (UINTPTR_MAX)
    auto sentry = Entry {now, std::unique_ptr<Timer, TimerDeleter>{std::bit_cast<Timer*>(UINTPTR_MAX), doNonthingDeleter_}};

    // 找到第一个 >= sentry 的元素
    auto stop_pos = timers_.lower_bound(sentry);

    //todo:incase of invalid memory free 
    std::ignore = sentry.second.release();

    assert(stop_pos == timers_.end() or now < stop_pos->first);

    // 将 [timers_.begin(), end) 范围内的元素（即所有已到期的）移动到 expired 向量
    auto it = timers_.begin();
    while (it != stop_pos)
    {
        auto& entry_to_move = const_cast<Entry&>(*it);
        expired_entrys.emplace_back(std::move(entry_to_move));
        // 从 timers_ 中移除这些已到期的元素
        it = timers_.erase(it);
    }

    // 同时从 activeTimers_ 中移除这些已到期的元素
    std::ranges::for_each(expired_entrys, [this](const auto& it) {
        auto n = this->active_timers_.erase(it.second->getTimerId());
        assert(n == 1);
        (void)n;
    });
    assert(timers_.size() == active_timers_.size());
    return expired_entrys;
}

// 在处理完一批到期的定时器后，reset 方法负责：
// 1. 对于那些是重复执行 (it.second->repeat()) 且在回调执行期间未被 cancelingTimers_ 标记为取消的定时器，调用 Timer:: restart(now) 更新其下一次到期时间，并将其重新调用 insert() 方法插入到 timers_ 和 activeTimers_ 中。
// 2. 对于非重复的或已被取消的定时器，则 delete it.second 释放 Timer 对象内存。
// 3. 根据 timers_ 中新的最早到期时间（如果列表不为空），调用 detail::resetTimerfd 重新设置 timerfd_ 的下一次超时。
void TimerQueue::reset_(std::vector<Entry>& expired, Timestamp now)
{
    // const auto& [ex_time, timer]
    for (auto& [ex_time, timer] : expired)
    {
        // 对于那些是重复执行 (it.second->repeat()) 且在回调执行期间未被 cancelingTimers_ 标记为取消的定时器, 重新加入定时器队列中
        if (timer->isRepeatable()
            and (not canceling_timers_.contains(timer->getTimerId())))
        {
            timer->restart(now);
            insert_(std::move(timer));
        }
        else // 对于非重复的或已被取消的定时器
        {
            // FIXME move to a free list
            // 暗示未来可能考虑使用对象池 (free list) 来复用 Timer 对象，
            // 以减少频繁 new 和 delete 带来的开销和内存碎片。
            timer.reset(); // FIXME: no delete please
        }
    }

    auto next_expire = Timestamp {};
    if (not timers_.empty()) // 如果还有未到期的定时器
    {
        next_expire = timers_.begin()->second->getExpiration();
    }

    if (next_expire.valid()) // 如果存在下一个有效到期时间
    {
        resetTimerfd(timerfd_, next_expire);
    }
}

auto TimerQueue::insert_(std::unique_ptr<Timer, TimerDeleter> timer)
    -> std::pair<bool, Timestamp>
{
    owner_loop_->assertInOwnerThread();
    assert(timers_.size() == active_timers_.size());
    auto earliest_changed = false;
    auto expire_time      = timer->getExpiration();
    auto it               = timers_.begin();

    //  如果 timers_ 为空，或者新定时器的到期时间早于当前最早的定时器
    if (it == timers_.end() || expire_time < it->first)
    {
        earliest_changed = true;
    }

    // 将定时器也加入 active_timers_ 中
    {
        auto result = active_timers_.insert(std::make_pair(timer->getTimerId(), timer.get()));
        assert(result.second);
        (void)result;
    }
    {
        auto result = timers_.insert(Entry {expire_time, std::move(timer)});
        assert(result.second);
        (void)result;
    }

    assert(timers_.size() == active_timers_.size());
    return {earliest_changed, timers_.begin()->first};
}
