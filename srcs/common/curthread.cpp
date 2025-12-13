#include "common/util.h"
#include "common/fiber.h"
#include "common/fiberexception.h"
#include <cassert>
#include <string>
#include <unistd.h>
namespace CurThr{

    static constexpr unsigned int c_not_set = -1;
    static thread_local uint64_t s_CachedTid = c_not_set ;
    static thread_local std::string  s_ThreadName = "UnKnown";

    auto GetId()
        -> std::jthread::id
    {
        if (s_CachedTid == c_not_set)
        {
            s_CachedTid = UtilT::getThreadId();
        }
        return std::jthread::id{pthread_t{s_CachedTid}};
    }

    auto GetName()
        -> std::string
    {
        return s_ThreadName;
    }

    auto SetName(std::string name)
        -> void
    {
        s_ThreadName = std::move(name);
    }
    
} //namespace CurThr


namespace CurThr {

static thread_local auto s_RunningFiber = std::weak_ptr<Fiber> {}; // weak_ptr to avoid circular reference
// static thread_local auto s_RunningFiber = static_cast<Fiber*>(nullptr); // why not make it sptr? maybe for performance
static thread_local auto s_MainFiber    = Sptr<Fiber> {nullptr};

auto SetMainFiber(Sptr<Fiber> fiber)
    -> void
{
    s_MainFiber = fiber;
}

auto GetMainFiber()
    -> Sptr<Fiber>
{
    return s_MainFiber;
}

auto GetRawMainFiber()
    -> Fiber*
{
    return s_MainFiber.get();
}

auto GetRunningFiberId()
    -> std::optional<uint64_t>
{
    if(auto fiber = GetRunningFiber(); fiber != nullptr)[[likely]]
    {
        return fiber->GetId();
    }
    return std::nullopt;
    // if (s_RunningFiber != nullptr)
    // {
    //     return s_RunningFiber->GetId();
    // }
    // may no fiber in some thread
    // return std::nullopt;
}

void SetRunningFiber(Sptr<Fiber> f)
{
    s_RunningFiber = f;
}

// void SetRunningFiber(Sptr<Fiber* f)
// {
//     s_RunningFiber = f;
// }

/**
 * @brief 返回当前线程正在执行的协程
 */
auto GetRunningFiber()
    -> Sptr<Fiber>
{
    return s_RunningFiber.lock();
    // return s_RunningFiber != nullptr ? s_RunningFiber->shared_from_this() : nullptr;
}

// auto GetRawRunningFiber()
//     -> Fiber*
// {
//     return s_RunningFiber;
// }

auto YieldToReady()
    -> void
{
    auto cur_fiber = GetRunningFiber();

    if (cur_fiber == nullptr) [[unlikely]]
    {
        throw NullPointerError {"No running Fiber here"};
    }

#ifndef NO_ASSERT
    assert(cur_fiber->GetState() == FiberState::RUNNING);
#endif

    cur_fiber->YieldTo_(FiberState::READY);
}

auto YieldToHold()
    -> void
{
    auto cur_fiber = GetRunningFiber();

    if (cur_fiber == nullptr) [[unlikely]]
    {
        throw NullPointerError {"No running Fiber here"};
    }

#ifndef NO_ASSERT
    assert(cur_fiber->GetState() == FiberState::RUNNING);
#endif
    if (cur_fiber) [[unlikely]]
    {
        throw NullPointerError {"No running Fiber here"};
    }
    cur_fiber->YieldTo_(FiberState::HOLD);
}

auto YieldToExcept()
    -> void
{
    auto cur_fiber = GetRunningFiber();
    if (cur_fiber == nullptr) [[unlikely]]
    {
        throw NullPointerError {"No running Fiber here"};
    }
#ifndef NO_ASSERT
    assert(cur_fiber->GetState() == FiberState::RUNNING);
#endif
    cur_fiber->YieldTo_(FiberState::EXCEPT);
}

auto YieldToTerm()
    -> void
{
    auto cur_fiber = GetRunningFiber();

#ifndef NO_ASSERT
#endif

    if (cur_fiber) [[unlikely]]
    {
        throw NullPointerError {"No running Fiber here"};
    }
    cur_fiber->YieldTo_(FiberState::TERM);
}

auto Resume(Sptr<Fiber> fiber)
    -> void
{

#ifndef NO_DEBUG
#endif
    auto cur_fiber = GetRunningFiber();
    if (cur_fiber == nullptr) [[unlikely]]
    {
        throw NullPointerError {"No Main Fiber running here"};
    }
    fiber->Resume_();
}

} // namespace CurThr