#include "timer/timer.h"
#include "common/mutex.h"
#include "common/util.h"
#include "common/util.hpp"
#include <functional>
#include <utility>

namespace TimerT{
    
auto Timer::Comparator::operator()(Sptr<Timer> lhs,
                                 Sptr<Timer> rhs)
    -> bool
{
    if(UtilT::Null(lhs) and UtilT::Null(rhs))
        return false;

    if (UtilT::Null(lhs)) return true;
    if (UtilT::Null(rhs)) return false;

    if(lhs->expired_time_ != rhs->expired_time_)
        return lhs->expired_time_ < rhs -> expired_time_;

    return lhs.get() < rhs.get();
}

Timer::Timer(uint64_t ms,
             std::function<void()> cb,
             bool recurring,
             TimerManager* manager)
    : recurring_ {recurring}
    , interval_ {ms}
    , expired_time_{UtilT::GetElapseMs() + ms}
    , callback_ {std::move(cb)}
    , manager_ {manager}

{

}

Timer::Timer(uint64_t expired_time)
    : expired_time_(expired_time)
{
}

auto Timer::Cancel()
    -> bool
{
    auto _ = Cot::LockGuard<Cot::RWMutex, Cot::WriteLockTag> {manager_->mtx_};
    if ( callback_ )
    {
        callback_ = nullptr;
        manager_->timers_.erase(this->shared_from_this());
        return true;
    }
    return false;
}

auto Timer::Refresh()
    -> bool
{
    auto _ = Cot::LockGuard<Cot::RWMutex, Cot::WriteLockTag> {manager_->mtx_};

    if(! callback_) {
        return false;
    }
    auto it = manager_->timers_.find(shared_from_this());
    if(it == manager_->timers_.end()) {
        return false;
    }

    manager_->timers_.erase(it);
    expired_time_ = UtilT::GetElapseMs() + interval_;

    manager_->timers_.insert(shared_from_this());
    return true;
}

auto Timer::Reset(uint64_t new_interval, bool from_now)
    -> bool
{
    if(new_interval == interval_ && not from_now) {
        return true;
    }
    manager_->mtx_.Wrlock();

    if(! callback_) {
        return false;
    }

    auto it = manager_->timers_.find(shared_from_this());
    if(it == manager_->timers_.end()) {
        return false;
    }
    manager_->timers_.erase(it);
    uint64_t start = 0;
    if(from_now) {
        start = UtilT::GetElapseMs();
    } else {
        start = expired_time_ - interval_;
    }
    interval_ = new_interval;
    expired_time_ = start + interval_;
    manager_->AddTimer(shared_from_this(), manager_->mtx_);
    return true;

}

TimerManager::TimerManager() {
    pre_time_ = UtilT::GetElapseMs();
}

TimerManager::~TimerManager() {
}

Sptr<Timer> TimerManager::AddTimer(uint64_t ms, std::function<void()> cb
                                  ,bool recurring) {
    Sptr<Timer> timer(new Timer(ms, cb, recurring, this));
    mtx_.Wrlock();
    AddTimer(timer, mtx_);
    return timer;
}

static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb) {
    std::shared_ptr<void> tmp = weak_cond.lock();
    if(tmp) {
        cb();
    }
}

Sptr<Timer> TimerManager::AddConditionTimer(uint64_t ms, std::function<void()> cb
                                    ,std::weak_ptr<void> weak_cond
                                    ,bool recurring) {

    return AddTimer(ms,
                    [weak_cond, cb = std::move(cb)]() -> void { OnTimer(weak_cond, cb); }
                    , recurring);
    // return AddTimer(ms, std::bind(&OnTimer, weak_cond, cb), recurring);
}

uint64_t TimerManager::GetIntervalToNextTimer() {
    auto _ = Cot::LockGuard<Cot::RWMutex, Cot::ReadLockTag> {mtx_};
    tickled_ = false;
    if(timers_.empty()) {
        return ~0ull;
    }
    Sptr<Timer> next = *timers_.begin();
    uint64_t now_ms = UtilT::GetElapseMs();
    return now_ms >= next->expired_time_ ? 0 : next->expired_time_ - now_ms;
}

void TimerManager::ListExpiredTimerCallbacks(std::vector<std::function<void()> >& cbs) {
    uint64_t now_ms = UtilT::GetElapseMs();
    std::vector<Sptr<Timer>> expired;
    auto _ = Cot::LockGuard<Cot::RWMutex, Cot::WriteLockTag> { mtx_ };
    if(timers_.empty()) {
        return;
    }
    bool rollover = false;
    if(DetectClockRollover_(now_ms)) [[unlikely]]{
        // 使用clock_gettime(CLOCK_MONOTONIC_RAW)，应该不可能出现时间回退的问题
        rollover = true;
    }
    if(!rollover && ((*timers_.begin())->expired_time_ > now_ms)) {
        return;
    }

    Sptr<Timer> now_timer(new Timer{ now_ms });

    auto it = rollover ? timers_.end() : timers_.lower_bound(now_timer);

    while(it != timers_.end() && (*it)->expired_time_ == now_ms) {
        ++it;
    }
    expired.insert(expired.begin(), timers_.begin(), it);
    timers_.erase(timers_.begin(), it);
    cbs.reserve(expired.size());

    for(auto& timer : expired) {
        cbs.push_back(timer->callback_);
        if(timer->recurring_) {
            timer->expired_time_ = now_ms + timer->interval_;
            timers_.insert(timer);
        } else {
            timer->callback_ = nullptr;
        }
    }
}

void TimerManager::AddTimer(Sptr<Timer> val, Cot::RWMutex& lock) {
    auto it = timers_.insert(val).first;
    bool at_front = (it == timers_.begin()) && !tickled_;
    if(at_front) {
        tickled_ = true;
    }
    lock.Unlock();

    if(at_front) {
        OnTimerInsertedAtFront();
    }
}

bool TimerManager::DetectClockRollover_(uint64_t now_ms) {
    bool rollover = false;
    if(now_ms < pre_time_ &&
            now_ms < (pre_time_ - 60 * 60 * 1000)) {
        rollover = true;
    }
    pre_time_ = now_ms;
    return rollover;
}

auto TimerManager::HasTimer()
    -> bool
{
    auto _ = Cot::LockGuard<Cot::RWMutex, Cot::ReadLockTag> {mtx_};
    return !timers_.empty();
}


} //namespace TimerT