#include "fiber/fiber.h"
#include "common/util.hpp"
#include "fiber/fiberexception.h"
#include "fiber/fiberstate.h"
#include "logger/logger.h"
#include "logger/loggermanager.h"
#include "fiber/fiberutil.h"

#include <atomic>
#include <boost/math/policies/policy.hpp>
#include <cassert>
#include <cstdlib>
#include <exception>
#include <memory>
#include <ucontext.h>

namespace FiberT{

static thread_local auto t_RunningFiber = (Fiber*)nullptr;  //why not make it sptr? maybe for performance
static thread_local auto t_MainFiber = Sptr<Fiber>{nullptr};
static auto s_RunningFiberCount = std::atomic<uint64_t>{0};
static auto s_FiberId = std::atomic<uint64_t>{0};

Sptr<LogT::Logger> g_logger = GET_ROOT_LOGGER();

static auto SetCurThrMainThread(Sptr<Fiber> fiber)
    -> void
{
    t_MainFiber = fiber;
    LOG_DEBUG(g_logger) << " not here";
}

class StackAllocator {
public:
    static auto Alloc(std::size_t size)
        -> void*
    {
        return malloc(size);
    }
    static auto Dealloc(void* vp)
        -> void
    {
        return free(vp);
    }

};

auto Fiber::GetCurThrRunningFiberId()
    -> uint64_t
{
    //因为有的线程不一定有协程
    return t_RunningFiber != nullptr ? t_RunningFiber->id_ : 0;
}

Fiber::Fiber()
    : stk_(nullptr, StackAllocator::Dealloc)
{
    assert(t_MainFiber == nullptr);
    if (auto success = UtilT::SyscallWrapper<-1>(getcontext, &ctx_);not success) [[unlikely]]
    {
        throw FiberInitError{"Init the main fiber failed -- " + success.error()};
    }

    SetState(FiberState::EXEC);
    SetCurThrRunningFiber(this);
    // SetCurThrMainThread(this->shared_from_this()); can`t call this->shared_from_this() here
    ++ s_RunningFiberCount;
    //todo: log here?
}

Fiber::Fiber(std::function<void()> cb,
             std::uint32_t stk_size,
             bool use_caller)
    : id_ { ++s_FiberId }
    , stk_size_ { stk_size == 0 ? c_DefaultStkSize : stk_size }
    , stk_ {StackAllocator::Alloc(stk_size_), StackAllocator::Dealloc}
    , cb_{ std::move(cb) }

{
    ++s_RunningFiberCount;
    if (auto success = UtilT::SyscallWrapper<-1>(getcontext, &ctx_);
        not success) [[unlikely]]
    {
        throw FiberInitError{"Init the main fiber failed -- " + success.error()};
    }

    ctx_.uc_link = nullptr;
    ctx_.uc_stack.ss_sp = stk_.get();
    ctx_.uc_stack.ss_size = stk_size_;

    if (not use_caller)
    {
        makecontext(&ctx_, SubFiberMain,0);
    }
    else
    {
        // makecontext(&ctx_, CallerMainFunc, 0);
    }
    //todo:log here
}

Fiber::~Fiber()
{
    LOG_INFO(g_logger) << "~Fiber:" << IsMainFiber_() << FiberStateToString(state_);

    -- s_RunningFiberCount;

    if (not IsMainFiber_())
    {
        
        LOG_INFO(g_logger) << "FiberState::" << FiberStateToString(state_);
        //todo log ?
        assert(state_ == FiberState::TERM
               or state_ == FiberState::EXCEPT
               or state_ == FiberState::INIT);
        return;
    }

    assert(not cb_);
    assert(state_ == FiberState::EXEC);
    auto *cur = t_RunningFiber;
    assert(cur == this);

    SetCurThrRunningFiber(nullptr);
    //todo: log here
}

auto Fiber::Reset(std::function<void()> cb)
    -> void
{
    // todo: assset here
    assert(stk_ != nullptr); //main fiber won`t call this function
    assert(state_ == FiberState::TERM);
    assert(state_ == FiberState::EXCEPT);
    assert(state_ == FiberState::INIT);
    cb_ = std::move(cb);

    if (auto success = UtilT::SyscallWrapper<-1>(getcontext, &ctx_);not success) [[unlikely]]
    {
        throw FiberInitError{"reset the main fiber failed -- " + success.error()};
    }

    ctx_.uc_link = nullptr;
    ctx_.uc_stack.ss_sp = stk_.get();
    ctx_.uc_stack.ss_size = stk_size_;
    
    SetState(FiberState::INIT);
}

auto Fiber::CreateMainFiber()
    -> void
{
    auto main_fiber = CreateFiber_();
    SetCurThrMainThread(main_fiber->shared_from_this()); 
}

auto Fiber::CreateSubFiber(std::function<void()> cb,
                           std::uint32_t stk_size,
                          bool use_caller)
    -> std::shared_ptr<Fiber>
{
    return CreateFiber_(cb, stk_size, use_caller);
}

auto Fiber::Resume()
    -> void
{
    assert(state_ != FiberState::EXEC);

    SetState(FiberState::EXEC);

    SetCurThrRunningFiber(this);
    if (auto success = UtilT::SyscallWrapper<-1>(swapcontext, &t_MainFiber->ctx_, &ctx_); not success) [[unlikely]]
    {
        throw FiberSwapError{"swap out main fiber failed -- " + success.error()};
        //todo assert here
    }
}


auto Fiber::BackToMain_(FiberState next_state)
    -> void
{
    SetState(next_state);
    SetCurThrRunningFiber(t_MainFiber.get());
	
    if (auto success = UtilT::SyscallWrapper<-1>(swapcontext, &ctx_, &t_MainFiber->ctx_); not success) [[unlikely]]
    {
        //todo assert here
        throw FiberSwapError{"swap in main fiber failed -- " + success.error()};
    }
}


void Fiber::SetCurThrRunningFiber(Fiber* f)
{
    t_RunningFiber = f;
}

auto Fiber::GetCurThrRunningFiber()
    -> Sptr<Fiber>
{
    if(t_RunningFiber == nullptr)
        CreateMainFiber();

    return t_RunningFiber->shared_from_this();
} 


auto Fiber::YieldToReady()
    -> void
{
    auto cur = GetCurThrRunningFiber();
    assert(cur->GetState() == FiberState::EXEC);
    cur->BackToMain_(FiberState::READY);
}

auto Fiber::YieldToHold()
    -> void
{
    auto cur = GetCurThrRunningFiber();
    assert(cur->GetState() == FiberState::EXEC);
    cur->BackToMain_(FiberState::HOLD);
}

auto Fiber::TotalFibers()
    -> uint64_t
{
    return s_RunningFiberCount;
}

auto Fiber::SubFiberMain()
    -> void
{
    auto cur = GetCurThrRunningFiber();
    assert(cur != nullptr);
    assert(cur->state_ == FiberState::EXEC);
    try
    {
        cur->cb_();
        cur->cb_ = nullptr; // to release the resource the cb_ may hava
        cur->state_ = FiberState::TERM;
    } catch (std::exception& ex)
    {
        cur->state_ = FiberState::EXCEPT;
        // todo:: log here
    } catch (...)
    {
        cur->state_ = FiberState::EXCEPT;
        // todo: log here
    }

    // -- s_RunningFiberCount; only ~Fiber() will do this, cause you may reuse this fiber before ~Fiber()

    auto raw_ptr = cur.get();
    cur.reset(); // why should call reset() here ? if you don`t, the ref count of this fiber won`t
                 // be 0 cause the
                 // why the raw_ptr won`t call destructor here after cur.reset()? cause the handle of this fiber is still hold by main fiber, and the handle always be a Sptr of Fiber

    LOG_INFO(g_logger) << "from SubFiberMain go back to main:" << FiberStateToString(raw_ptr->GetState());
    raw_ptr->BackToMain_(raw_ptr->GetState());
    //todo: log here

}

auto Fiber::IsMainFiber_() const
    -> bool
{
    return stk_size_ == 0;
}

} //namespace FiberT