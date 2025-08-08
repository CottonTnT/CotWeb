#include "common/allocator.h"
#include "common/util.h"
#include "common/util.hpp"
#include "fiber/fiberexception.h"
#include "fiber/fiberstate.h"
#include "fiber/scheduler.h"
#include "logger/logger.h"
#include "fiber/fiber.h"
#include "logger/loggermanager.h"
#include "fiber/fiberutil.h"

#include <atomic>
#include <boost/math/policies/policy.hpp>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <memory>
#include <optional>
#include <ucontext.h>
#include <iostream>

static Sptr<LogT::Logger> g_logger = GET_ROOT_LOGGER();
namespace CurThr {

using namespace FiberT;

static thread_local auto s_RunningFiber = static_cast<Fiber*>(nullptr); // why not make it sptr? maybe for performance
static thread_local auto s_MainFiber    = Sptr<Fiber> {nullptr};

static auto SetMainFiber(Sptr<Fiber> fiber)
    -> void
{
    s_MainFiber = fiber;
}

static auto GetMainFiber()
    -> Sptr<Fiber>
{
    return s_MainFiber;
}

auto GetRunningFiberId()
    -> std::optional<uint64_t>
{
    if (s_RunningFiber != nullptr)
    {
        return s_RunningFiber->GetId();
    }
    // may no fiber in some thread
    return std::nullopt;
}

void SetRunningFiber(Fiber* f)
{
    s_RunningFiber = f;
}

/**
 * @brief 返回当前线程正在执行的协程
 */
auto GetRunningFiber()
    -> Sptr<Fiber>
{
    return s_RunningFiber != nullptr ? s_RunningFiber->shared_from_this()
                                     : nullptr;
}

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

    cur_fiber->YieldTo(FiberState::READY);
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
    cur_fiber->YieldTo(FiberState::HOLD);
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
    cur_fiber->YieldTo(FiberState::EXCEPT);
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
    cur_fiber->YieldTo(FiberState::TERM);
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
    fiber->Resume();
}

} // namespace CurThr

namespace FiberT {

static auto s_RunningFiberCount = std::atomic<uint64_t> {0};
static auto s_FiberId           = std::atomic<uint64_t> {0};

// note: 踩坑,注意静态变量的析构顺序，保证g_logger在析构函数打印日志时，最后一个析构
// Sptr<LogT::Logger> g_logger = GET_ROOT_LOGGER(); 需定义为

Fiber::Fiber()
    : id_ {s_FiberId++}
    , state_ {FiberState::RUNNING}
    , stk_ {
          nullptr,
          [](void*) -> void {}}
{
#ifndef NO_ASSERT
    assert(CurThr::s_MainFiber == nullptr);
#endif

    if (auto success = UtilT::SyscallWrapper<-1>(getcontext, &ctx_); not success) [[unlikely]]
    {
        throw InitError {"Init the main fiber failed -- " + success.error()};
    }

    CurThr::SetRunningFiber(this);
    // SetCurThrMainThread(this->shared_from_this()); can`t call this->shared_from_this() here
    ++s_RunningFiberCount;

#ifndef NO_DEBUG
    LOG_DEBUG(g_logger) << "Fiber::Fiber() main id = " << id_;
#endif
}

Fiber::Fiber(std::function<void()> cb,
             std::uint32_t stk_size)
    : id_ {s_FiberId++}
    , stk_size_ {stk_size == 0 ? c_DefaultStkSize : stk_size}
    , state_ {FiberState::READY}
    , stk_ {StackAllocator::Alloc(stk_size_), StackAllocator::Dealloc}
    , callback_ {std::move(cb)}
// , run_in_scheduler_(run_in_scheduler)

{
    if (auto success = UtilT::SyscallWrapper<-1>(getcontext, &ctx_);
        not success) [[unlikely]]
    {
        throw InitError {"Init the main fiber failed -- " + success.error()};
    }
    ++s_RunningFiberCount;

    ctx_.uc_link          = nullptr;
    ctx_.uc_stack.ss_sp   = stk_.get();
    ctx_.uc_stack.ss_size = stk_size_;

    makecontext(&ctx_, CallBackWrapper, 0);

#ifndef NO_DEBUG
    LOG_DEBUG(g_logger) << "Fiber::Fiber() sub id = " << id_;
#endif
}

Fiber::~Fiber()
{
#ifndef NO_DEBUG
    LOG_DEBUG(g_logger) << std::format("fiber {} : fiber::~Fiber() start to destruct", CurThr::GetRunningFiberId().value());
#endif

    --s_RunningFiberCount;

    if (not IsMainFiber_())
    {

#ifndef NO_ASSERT
        assert(state_ == FiberState::TERM
               || state_ == FiberState::EXCEPT
               || state_ == FiberState::INIT);
#endif
    }
    else
    {
#ifndef NO_ASSERT
        assert(not callback_);
        assert(state_ == FiberState::RUNNING); // 此时主线程一定是执行状态
#endif

#ifndef NO_ASSERT
        auto* cur = CurThr::s_RunningFiber;
        assert(cur == this); // 我认为，非对称的协程模型，
                             // 主协程释放时，其他子协程必须已经释放
                             //  sylar做法如下，暂时觉得有点多余
                             //  Fiber* cur = CurThr::s_RunningFiber;
                             //  if(cur == this) {
                             //      SetThis(nullptr);
                             //  }
#endif
        CurThr::SetRunningFiber(nullptr); // in case get the trash value
    }

}
// 切换到当前协程执行
void Fiber::Resume()
{

#ifndef NO_DEBUG
    LOG_DEBUG(g_logger) << std::format("fiber {} : Fiber::Resume()", GetId());
#endif
    CurThr::SetRunningFiber(this);
#ifndef NO_ASSERT
    assert(state_ != FiberState::RUNNING);
#endif
    SetState(FiberState::RUNNING);
    if (auto success = UtilT::SyscallWrapper<-1>(swapcontext, &(CurThr::s_MainFiber->ctx_), &ctx_); not success) [[unlikely]]
    {
        throw SwapError {std::format("swap to the fiber {} failed: {}", GetId(), success.error())};
    }
}

auto Fiber::Reset(std::function<void()> cb)
    -> void
{

#ifndef NO_DEBUG
    LOG_DEBUG(g_logger) << std::format("fiber {} : Fiber::Reset()", GetId());
#endif

#ifndef NO_ASSERT
    assert(IsMainFiber_()); // main fiber won`t call this function
    assert(state_ == FiberState::TERM
           || state_ == FiberState::EXCEPT
           || state_ == FiberState::INIT);
#endif

    // reset the ucontext
    if (auto success = UtilT::SyscallWrapper<-1>(getcontext, &ctx_); not success) [[unlikely]]
    {
        throw InitError {"reset the main fiber failed -- " + success.error()};
    }

    ctx_.uc_link          = nullptr;
    ctx_.uc_stack.ss_sp   = stk_.get();
    ctx_.uc_stack.ss_size = stk_size_;
    makecontext(&ctx_, &Fiber::CallBackWrapper, 0);

    // reset the Fiber state
    callback_ = std::move(cb);
    SetState(FiberState::INIT);
}

auto Fiber::CreateFiber(std::function<void()> cb,
                        std::uint32_t stk_size)
    -> std::shared_ptr<Fiber>
{
    if (CurThr::GetMainFiber() == nullptr)
    {
        auto main_fiber = CreateFiberImpl_();
        CurThr::SetMainFiber(main_fiber);
    }

    return CreateFiberImpl_(cb, stk_size);
}

auto Fiber::YieldTo(FiberState next_state)
    -> void
{

#ifndef NO_DEBUG
    LOG_DEBUG(g_logger) << std::format("fiber {} : Fiber::YieldTo():from {}->{}", GetId(), FiberStateToString(GetState()), FiberStateToString(next_state));
#endif
    /// 协程运行完之后会自动yield一次，用于回到主协程，此时状态已为结束状态
    this->SetState(next_state);
    CurThr::SetRunningFiber(CurThr::s_MainFiber.get());

    if (auto success = UtilT::SyscallWrapper<-1>(swapcontext, &ctx_, &(CurThr::s_MainFiber->ctx_)); not success) [[unlikely]]
    {

        throw SwapError {"swap to the main fiber failed -- " + success.error()};
    }
    // talk about:  what if a TERM fiber call resume ? how about just return ?
    // for scheduler in OS, never rerun the dead proc, so the developer should
    // responseble for the correctness cause you are the scheduler now!

    // talk about: how about set the ctx_->uc_link = t_MainFiber->ctx_ here? a nice choice or a bad one ?
    // I don`t know yet.
}

auto Fiber::GetTotalFibers()
    -> uint64_t
{
    return s_RunningFiberCount;
}

auto Fiber::CallBackWrapper()
    -> void
{
    auto cur = CurThr::GetRunningFiber();
#ifndef NO_ASSERT
    assert(cur != nullptr);
#endif
#ifndef NO_DEBUG
    LOG_DEBUG(g_logger) << std::format("fiber {} : fiber::CallBackWrapper() start", CurThr::GetRunningFiberId().value());
#endif
    try
    {
        cur->callback_();
        cur->callback_ = nullptr;
        cur->SetState(FiberState::TERM);
    } catch (std::exception& ex)
    {

        cur->SetState(FiberState::EXCEPT);
        LOG_ERROR(g_logger) << "Fiber Except: "
                            << ex.what()
                            << " fiber_id=" << cur->GetId()
                            << UtilT::BacktraceToString();
    } catch (...)
    {
        cur->SetState(FiberState::EXCEPT);
        LOG_ERROR(g_logger) << "Fiber Except"
                            << " fiber_id=" << cur->GetId()
                            << UtilT::BacktraceToString();
    }

    /*
     * talk about: 这里是否应该删减处理协程函数出现异常的情况 ?
     * 有人认为协程的异常不应该由框架处理，应该由开发者自行处理, 理由是平时线程出现异常时
     * 开发者必须处理，不然就crash
     * 我认为不该如此，类比OS里的thread，我们的就是 fiber 的OS，调度器角色，难道你会因为
     * 你其中的一个线程崩掉，就crash 整个 OS吗，absolutly not！
     */

    // -- s_RunningFiberCount; only ~Fiber() will do this, cause you may reuse this fiber before ~Fiber()
    auto* raw_ptr = cur.get();
    cur.reset();
    // why call reset() here ? if you don`t, the ref count of this fiber won`t be 0(RAII won`t work)
    // If the the fiber destructor will be call after cur.reset()?

    // No, cause the handle of this fiber is still hold by main fiber, and the handle always be a Sptr of Fiber

#ifndef NO_DEBUG
    LOG_DEBUG(g_logger) << std::format("fiber {} : fiber::CallBackWrapper() back To Main Fiber", CurThr::GetRunningFiberId().value());
#endif

    raw_ptr->YieldTo(raw_ptr->GetState());

    LOG_ERROR(g_logger) << "should never been here!";
    assert(false); // should never reach here
}

auto Fiber::IsMainFiber_() const
    -> bool
{
    return stk_size_ == 0;
}

} // namespace FiberT