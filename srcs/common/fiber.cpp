#include "common/allocator.h"
#include "common/curthread.h"
#include "common/util.h"
#include "common/util.hpp"
#include "common/fiberexception.h"
#include "common/fiberstate.h"
#include "logger/logger.h"
#include "common/fiber.h"
#include "logger/loggermanager.h"
#include "common/fiberutil.h"

#include <atomic>
#include <boost/math/policies/policy.hpp>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <memory>
#include <optional>
#include <ucontext.h>
#include <iostream>

static Sptr<LogT::Logger> g_logger = GET_ROOT_LOGGER();

namespace FiberT {

static auto s_RunningFiberCount = std::atomic<uint64_t> {0};
static auto s_FiberId           = std::atomic<uint64_t> {0};

// note: 踩坑,注意静态变量的析构顺序，保证g_logger在析构函数打印日志时，最后一个析构
// Sptr<LogT::Logger> g_logger = GET_ROOT_LOGGER(); 需定义为

Fiber::Fiber()
    : id_ {++s_FiberId}
    , state_ {FiberState::RUNNING}
    , stk_ {
          nullptr,
          [](void*) -> void {}}
{
#ifndef NO_ASSERT
    assert(CurThr::GetMainFiber() == nullptr);
#endif

    if (auto success = UtilT::SyscallWrapper<-1>(getcontext, &ctx_); not success) [[unlikely]]
    {
        throw InitError {"Init the main fiber failed -- " + success.error()};
    }

    // CurThr::SetRunningFiber(this);
    // SetCurThrMainThread(this->shared_from_this()); can`t call this->shared_from_this() here
    ++s_RunningFiberCount;

#ifndef NO_DEBUG
    LOG_DEBUG(g_logger) << "Fiber::Fiber() main id = " << id_;
#endif
}

// todo:deal with the fiber id
Fiber::Fiber(std::function<void()> cb,
             std::uint32_t stk_size)
    : id_ {cb ? ++s_FiberId : c_NonUse}
    , stk_size_ {stk_size == 0 ? c_DefaultStkSize : stk_size}
    , state_ {cb ? FiberState::READY : FiberState::UNUSED}
    , stk_ {StackAllocator::Alloc(stk_size_), StackAllocator::Dealloc}
    , callback_ {std::move(cb)}
{
    if (auto success = UtilT::SyscallWrapper<-1>(getcontext, &ctx_);
        not success) [[unlikely]]
    {
        throw InitError {"Init the main fiber failed -- " + success.error()};
    }
    if (cb)
        ++s_RunningFiberCount;

    ctx_.uc_link = nullptr;
    // talk about: how to set the uc_link like below so that over resume will back to main fiber?
    // !no, still wrong, the pc counter changed that you will run in chaos
    // ctx_.uc_link          = &(CurThr::s_MainFiber->ctx_);
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
    LOG_DEBUG(g_logger) << std::format("fiber {} : fiber::~Fiber() start to destruct", id_);
#endif

    //todo: is it suitable to manage the s_RunningFiberCount here?
    if (state_ != FiberState::UNUSED)
        --s_RunningFiberCount;

    if (not IsRootFiber_())
    {
#ifndef NO_ASSERT
        assert(state_ == FiberState::TERM
               || state_ == FiberState::EXCEPT
               || state_ == FiberState::UNUSED);
        // talk about: why no HOLD here ? cause we do not support to kill a fiber
#endif
    }
    else // for root fiber that use the current thread stack
    {
#ifndef NO_ASSERT
        assert(state_ == FiberState::RUNNING); // 此时主线程一定是执行状态
#endif

#ifndef NO_ASSERT
        assert(CurThr::GetRunningFiber() == nullptr);
                             // 我认为，非对称的协程模型，
                             // root协程释放时，本线程就不应当有其他运行的协程
#endif
        CurThr::SetRunningFiber(nullptr); // in case get the trash value
    }
}
// 切换到当前协程执行
void Fiber::Resume_()
{

#ifndef NO_DEBUG
    LOG_DEBUG(g_logger) << std::format("fiber {} : Fiber::Resume()", GetId());
#endif

#ifndef NO_ASSERT
    assert(state_ == FiberState::READY
           or state_ == FiberState::HOLD);
    assert(not IsMainFiber_());
#endif

    // CurThr::SetRunningFiber(this);
    CurThr::SetRunningFiber(this->shared_from_this());
    SetState(FiberState::RUNNING);

    if (auto success = UtilT::SyscallWrapper<-1>(swapcontext, &(CurThr::GetRawMainFiber()->ctx_), &ctx_); not success) [[unlikely]]
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
    // assert(not IsMainFiber_()); // main fiber won`t call this function
    assert(state_ == FiberState::TERM
           || state_ == FiberState::EXCEPT
           || state_ == FiberState::UNUSED);
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

    SetState(cb ? FiberState::READY : FiberState::UNUSED);
}

auto Fiber::CreateMainFiber_()
    -> Sptr<Fiber>
{
    auto main_fiber = CreateFiberImpl_();
    CurThr::SetMainFiber(main_fiber);
    CurThr::SetRunningFiber(main_fiber);
    return main_fiber;
}

auto Fiber::CreateFiber(std::function<void()> cb,
                        std::uint32_t stk_size)
    -> std::shared_ptr<Fiber>
{
    if (CurThr::GetMainFiber() == nullptr)
    {
        CreateMainFiber_();
    }

    return CreateFiberImpl_(cb, stk_size);
}

auto Fiber::YieldTo_(FiberState next_state)
    -> void
{

#ifndef NO_DEBUG
    LOG_DEBUG(g_logger) << std::format("fiber {} : Fiber::YieldTo():from {}->{}", GetId(), FiberStateToString(GetState()), FiberStateToString(next_state));
#endif
    this->SetState(next_state);

    // CurThr::SetRunningFiber(CurThr::GetMainFiber().get());
    CurThr::SetRunningFiber(CurThr::GetMainFiber());

    //sorry, must use raw ptr here, I try my best
    if (auto success = UtilT::SyscallWrapper<-1>(swapcontext, &ctx_, &(CurThr::GetRawMainFiber()->ctx_)); not success) [[unlikely]]
    {
        throw SwapError {"swap to the main fiber failed -- " + success.error()};
    }
    // talk about:  what if a TERM fiber call resume ? how about just return ?
    // for scheduler in OS, never rerun the dead proc, so the developer should
    // responseble for the correctness cause you are the scheduler now!

    // talk about: how about set the ctx_->uc_link = t_MainFiber->ctx_ here? a nice choice or a bad one ?
    // I don`t know yet.
#ifndef NO_DEBUG
    LOG_DEBUG(g_logger) << std::format("fiber {} {}: Fiber::YieldTo():bottom already term... back to Fiber::CallBackWrapper", GetId(), FiberStateToString(GetState()));
#endif
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
    assert(cur != nullptr and cur->GetState() == FiberState::RUNNING);
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
     * 协程确实应该负责好自身的任务处理，但时 调度器应该负责好协程的调度和管理，但协程中止
     * 时提它收尸
     */

    // -- s_RunningFiberCount; only ~Fiber() will do this, cause you may reuse this fiber before ~Fiber()
    auto* raw_ptr = cur.get(); //sorry, must use raw ptr here, I try my best
    cur.reset();

    // why call reset() here ? if you don`t, the ref count of this fiber won`t be 0(RAII won`t work)
    // If the the fiber destructor will be call after cur.reset()?

    // No, cause the handle of this fiber is still hold by main fiber, and the handle always be a Sptr of Fiber

#ifndef NO_DEBUG
    LOG_DEBUG(g_logger) << std::format("fiber {} : fiber::CallBackWrapper() back To Main Fiber", CurThr::GetRunningFiberId().value());
#endif

    raw_ptr->YieldTo_(raw_ptr->GetState());

    LOG_ERROR(g_logger) << "should never been here!";
    assert(false); // should never reach here
}

auto Fiber::IsMainFiber_() const
    -> bool
{
    return this == CurThr::GetMainFiber().get();
}

auto Fiber::IsRootFiber_() const
    -> bool
{
    return stk_size_ == 0;
}

} // namespace FiberT