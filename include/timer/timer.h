#include "common/alias.h"
#include "common/mutex.h"
#include <functional>
#include <memory>
#include <set>

namespace TimerT{
class TimerManager;

class Timer : public std::enable_shared_from_this<Timer> {
friend class TimerManager;
public:

    Timer(uint64_t ms,
             std::function<void()> cb,
             bool recurring,
             TimerManager* manager);


    explicit Timer(uint64_t next);
    /**
     * @brief cancel a timer
     */
    auto Cancel()
        -> bool;

    /**
     * @brief refresh the expires time of the timer
     */
    auto Refresh()
        -> bool;

    auto Reset(uint64_t ms, bool from_now)
        -> bool;

private:
    bool recurring_ = false; // 是否是周期性定时器
    uint64_t interval_    = 0;
    uint64_t expired_time_ = 0;
    std::function<void()>  callback_;
    TimerManager* manager_ = nullptr;

    struct Comparator
    {
        auto operator()(Sptr<Timer> lhs,
                        Sptr<Timer> rhs)
            -> bool;
    };
};

class TimerManager {
friend class Timer;
public:
    TimerManager();
    TimerManager(const TimerManager&)            = delete;
    TimerManager(TimerManager&&)                 = delete;
    TimerManager& operator=(const TimerManager&) = delete;
    TimerManager& operator=(TimerManager&&)      = delete;
    virtual ~TimerManager();

    auto AddTimer(uint64_t ms,
                  std::function<void()> cb,
                  bool recurring_)
        -> Sptr<Timer>;


    auto AddConditionTimer(uint64_t ms,
                           std::function<void()> cb,
                           std::weak_ptr<void> weak_cond,
                           bool recurring)
        -> Sptr<Timer>;

    auto GetIntervalToNextTimer()
        -> uint64_t;

    auto ListExpiredTimerCallbacks(std::vector<std::function<void()>>& cb)
        -> void;

    auto HasTimer()
        -> bool;

protected:
    /**
     * @brief 当有新的定时器插入到定时器函数时，执行该函数
     */
    virtual void OnTimerInsertedAtFront();

    auto AddTimer(Sptr<Timer> timer, Cot::RWMutex& lock)
        -> void;

private:
    auto DetectClockRollover_(uint64_t now_ms)
        -> bool;

private:
    Cot::RWMutex mtx_;
    std::set<Sptr<Timer>, Timer::Comparator> timers_; 
    bool tickled_ = false;
    uint64_t pre_time_ = 0;
};




} //namespace TimeT